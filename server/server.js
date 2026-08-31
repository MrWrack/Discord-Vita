import "dotenv/config";
import http from "node:http";
import crypto from "node:crypto";
import fs from "node:fs/promises";

const PORT = Number(process.env.PORT || 8080);
const CLIENT_ID = process.env.DISCORD_CLIENT_ID || "";
const CLIENT_SECRET = process.env.DISCORD_CLIENT_SECRET || "";
const BOT_TOKEN = process.env.DISCORD_BOT_TOKEN || "";
const STORE = process.env.SESSION_STORE || "./sessions.json";
const DEVICE_SCOPE = process.env.DISCORD_DEVICE_SCOPE || "identify guilds guilds.members.read";
const APP_SESSION_TTL_MS =
  Math.max(1, Number(process.env.APP_SESSION_TTL_DAYS || 30)) * 24 * 60 * 60 * 1000;
const API = "https://discord.com/api/v10";

const deviceLogins = new Map();
let sessions = new Map();

const now = () => Date.now();
const randomToken = () => crypto.randomBytes(32).toString("base64url");
const hashToken = (token) =>
  crypto.createHash("sha256").update(token, "utf8").digest("hex");

const send = (res, code, body) => {
  res.writeHead(code, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store",
    "x-content-type-options": "nosniff"
  });
  res.end(JSON.stringify(body));
};

function basicAuth() {
  return "Basic " + Buffer.from(`${CLIENT_ID}:${CLIENT_SECRET}`, "utf8").toString("base64");
}

function bearer(req) {
  const value = req.headers.authorization || "";
  return value.startsWith("Bearer ") ? value.slice(7) : "";
}

async function loadSessions() {
  try {
    const parsed = JSON.parse(await fs.readFile(STORE, "utf8"));
    sessions = new Map(Object.entries(parsed));
  } catch {
    sessions = new Map();
  }
}

let saveTimer = null;
function saveSessions() {
  clearTimeout(saveTimer);
  saveTimer = setTimeout(async () => {
    const temp = `${STORE}.tmp`;
    await fs.writeFile(
      temp,
      JSON.stringify(Object.fromEntries(sessions), null, 2),
      { mode: 0o600 }
    );
    await fs.rename(temp, STORE);
  }, 30);
}

async function discordJson(path, accessToken, options = {}) {
  const headers = {
    ...(options.headers || {}),
    Authorization: `Bearer ${accessToken}`
  };
  const response = await fetch(API + path, { ...options, headers });
  const text = await response.text();
  let body = {};
  try {
    body = text ? JSON.parse(text) : {};
  } catch {
    body = { raw: text };
  }
  return { status: response.status, ok: response.ok, body };
}


async function discordBot(path, options = {}) {
  if (!BOT_TOKEN) {
    return {
      status: 503,
      ok: false,
      body: { error: "bot_not_configured" }
    };
  }

  const headers = {
    ...(options.headers || {}),
    Authorization: `Bot ${BOT_TOKEN}`
  };

  const response = await fetch(API + path, { ...options, headers });
  const text = await response.text();
  let body = {};
  try {
    body = text ? JSON.parse(text) : {};
  } catch {
    body = { raw: text };
  }

  return { status: response.status, ok: response.ok, body };
}


const PERM_ADMINISTRATOR = 1n << 3n;
const PERM_VIEW_CHANNEL = 1n << 10n;
const PERM_SEND_MESSAGES = 1n << 11n;
const PERM_READ_MESSAGE_HISTORY = 1n << 16n;
const ALL_PERMISSIONS = (1n << 53n) - 1n;

function permissionBits(value) {
  try {
    return BigInt(value || "0");
  } catch {
    return 0n;
  }
}

function applyOverwrite(permissions, overwrite) {
  if (!overwrite) return permissions;
  permissions &= ~permissionBits(overwrite.deny);
  permissions |= permissionBits(overwrite.allow);
  return permissions;
}

function computeChannelPermissions({ guildId, ownerId, member, roles, channel }) {
  const userId = String(member?.user?.id || "");
  if (!userId) return 0n;
  if (ownerId && String(ownerId) === userId) return ALL_PERMISSIONS;

  const roleMap = new Map(
    (Array.isArray(roles) ? roles : []).map((role) => [String(role.id), role])
  );

  let permissions = permissionBits(roleMap.get(String(guildId))?.permissions);
  for (const roleId of member.roles || []) {
    permissions |= permissionBits(roleMap.get(String(roleId))?.permissions);
  }

  if ((permissions & PERM_ADMINISTRATOR) !== 0n) {
    return ALL_PERMISSIONS;
  }

  const overwrites = Array.isArray(channel?.permission_overwrites)
    ? channel.permission_overwrites
    : [];

  permissions = applyOverwrite(
    permissions,
    overwrites.find(
      (ow) => Number(ow.type) === 0 && String(ow.id) === String(guildId)
    )
  );

  let roleAllow = 0n;
  let roleDeny = 0n;
  const memberRoles = new Set((member.roles || []).map(String));

  for (const overwrite of overwrites) {
    if (Number(overwrite.type) !== 0) continue;
    if (!memberRoles.has(String(overwrite.id))) continue;
    roleAllow |= permissionBits(overwrite.allow);
    roleDeny |= permissionBits(overwrite.deny);
  }

  permissions &= ~roleDeny;
  permissions |= roleAllow;

  permissions = applyOverwrite(
    permissions,
    overwrites.find(
      (ow) => Number(ow.type) === 1 && String(ow.id) === userId
    )
  );

  return permissions;
}

async function loadGuildAccessContext(accessToken, guildId) {
  const [userGuilds, member, guild, roles, channels] = await Promise.all([
    discordJson("/users/@me/guilds", accessToken),
    discordJson(`/users/@me/guilds/${guildId}/member`, accessToken),
    discordBot(`/guilds/${guildId}`),
    discordBot(`/guilds/${guildId}/roles`),
    discordBot(`/guilds/${guildId}/channels`)
  ]);

  if (!userGuilds.ok || !Array.isArray(userGuilds.body)) {
    return { ok: false, status: userGuilds.status || 401, body: userGuilds.body };
  }

  const userGuild = userGuilds.body.find((g) => String(g.id) === String(guildId));
  if (!userGuild) {
    return {
      ok: false,
      status: 403,
      body: { error: "guild_not_authorized_for_user" }
    };
  }

  if (!member.ok) {
    return {
      ok: false,
      status: member.status === 403 ? 403 : member.status,
      body: member.status === 403
        ? { error: "guilds_members_read_scope_required" }
        : member.body
    };
  }

  for (const result of [guild, roles, channels]) {
    if (!result.ok) {
      return { ok: false, status: result.status, body: result.body };
    }
  }

  return {
    ok: true,
    userGuild,
    member: member.body,
    guild: guild.body,
    roles: roles.body,
    channels: channels.body
  };
}

function channelPermissionContext(ctx, channelId) {
  const channel = (ctx.channels || []).find(
    (c) => String(c.id) === String(channelId)
  );
  if (!channel) return null;

  const permissions = computeChannelPermissions({
    guildId: ctx.guild.id,
    ownerId: ctx.guild.owner_id,
    member: ctx.member,
    roles: ctx.roles,
    channel
  });

  return { channel, permissions };
}

async function userGuildIds(accessToken) {
  const result = await discordJson("/users/@me/guilds", accessToken);
  if (!result.ok || !Array.isArray(result.body)) return null;
  return new Set(result.body.map((g) => String(g.id)));
}

async function userCanAccessGuild(accessToken, guildId) {
  const ids = await userGuildIds(accessToken);
  return ids ? ids.has(String(guildId)) : false;
}

async function resolveBotChannelForUser(accessToken, channelId) {
  const channelResult = await discordBot(`/channels/${channelId}`);
  if (!channelResult.ok || !channelResult.body?.guild_id) {
    return {
      ok: false,
      status: channelResult.status,
      body: channelResult.body
    };
  }

  const guildId = String(channelResult.body.guild_id);
  const ctx = await loadGuildAccessContext(accessToken, guildId);
  if (!ctx.ok) return ctx;

  const access = channelPermissionContext(ctx, channelId);
  if (!access) {
    return {
      ok: false,
      status: 404,
      body: { error: "channel_not_available" }
    };
  }

  return {
    ok: true,
    guildId,
    channel: access.channel,
    permissions: access.permissions
  };
}

async function refreshDiscordToken(session) {
  if (!session.refreshToken) return false;

  const form = new URLSearchParams({
    grant_type: "refresh_token",
    refresh_token: session.refreshToken
  });

  const response = await fetch(`${API}/oauth2/token`, {
    method: "POST",
    headers: {
      Authorization: basicAuth(),
      "content-type": "application/x-www-form-urlencoded"
    },
    body: form
  });

  const token = await response.json().catch(() => ({}));
  if (!response.ok || !token.access_token) return false;

  session.accessToken = token.access_token;
  session.refreshToken = token.refresh_token || session.refreshToken;
  session.accessExpiresAt = now() + Number(token.expires_in || 604800) * 1000;
  saveSessions();
  return true;
}

async function authenticate(req) {
  const raw = bearer(req);
  if (!raw) return null;

  const key = hashToken(raw);
  const session = sessions.get(key);
  if (!session) return null;

  if (now() >= session.sessionExpiresAt) {
    sessions.delete(key);
    saveSessions();
    return null;
  }

  if (now() >= session.accessExpiresAt - 60_000) {
    if (!(await refreshDiscordToken(session))) {
      sessions.delete(key);
      saveSessions();
      return null;
    }
  }

  return { key, session };
}

async function beginDiscordDeviceLogin() {
  const form = new URLSearchParams({ scope: DEVICE_SCOPE });

  const response = await fetch(`${API}/oauth2/device/authorize`, {
    method: "POST",
    headers: {
      Authorization: basicAuth(),
      "content-type": "application/x-www-form-urlencoded"
    },
    body: form
  });

  const body = await response.json().catch(() => ({}));
  return { response, body };
}

async function exchangeDeviceCode(deviceCode) {
  const form = new URLSearchParams({
    grant_type: "urn:ietf:params:oauth:grant-type:device_code",
    device_code: deviceCode
  });

  const response = await fetch(`${API}/oauth2/token`, {
    method: "POST",
    headers: {
      Authorization: basicAuth(),
      "content-type": "application/x-www-form-urlencoded"
    },
    body: form
  });

  const body = await response.json().catch(() => ({}));
  return { response, body };
}

function cleanupDeviceLogins() {
  const t = now();
  for (const [id, item] of deviceLogins) {
    if (t >= item.expiresAt + 60_000) deviceLogins.delete(id);
  }
}
setInterval(cleanupDeviceLogins, 60_000).unref();

await loadSessions();

http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);

    if (req.method === "GET" && url.pathname === "/health") {
      return send(res, 200, { ok: true, name: "Discord Vita", version: "1.7.0" });
    }

    if (req.method === "GET" && url.pathname === "/capabilities") {
      const botReady = Boolean(BOT_TOKEN);
      return send(res, 200, {
        oauth_login: true,
        guilds: true,
        channels: botReady,
        messages: botReady,
        send_messages: botReady,
        send_identity: botReady ? "bot" : "disabled",
        reason: botReady
          ? "Channel and message access uses the configured Discord bot."
          : "Set DISCORD_BOT_TOKEN to enable channel and message access."
      });
    }

    if (req.method === "GET" && url.pathname === "/oauth/device/start") {
      if (!CLIENT_ID || !CLIENT_SECRET) {
        return send(res, 500, { error: "oauth_not_configured" });
      }

      const { response, body } = await beginDiscordDeviceLogin();
      if (!response.ok || !body.device_code || !body.user_code) {
        return send(res, 502, {
          error: "device_authorization_failed",
          discord_status: response.status,
          discord: body
        });
      }

      const transactionId = randomToken();
      const pollToken = randomToken();
      const interval = Math.max(2, Number(body.interval || 5));
      const expiresIn = Math.max(30, Number(body.expires_in || 300));

      deviceLogins.set(transactionId, {
        pollTokenHash: hashToken(pollToken),
        deviceCode: body.device_code,
        createdAt: now(),
        expiresAt: now() + expiresIn * 1000,
        nextPollAt: now() + interval * 1000,
        intervalMs: interval * 1000
      });

      return send(res, 200, {
        transaction_id: transactionId,
        poll_token: pollToken,
        user_code: body.user_code,
        verification_uri: body.verification_uri || "https://discord.com/activate",
        verification_uri_complete: body.verification_uri_complete || "",
        expires_in: expiresIn,
        interval
      });
    }

    if (req.method === "GET" && url.pathname === "/oauth/device/status") {
      const id = url.searchParams.get("id") || "";
      const item = deviceLogins.get(id);
      const pollToken = bearer(req);

      if (!item || !pollToken || hashToken(pollToken) !== item.pollTokenHash) {
        return send(res, 401, { error: "invalid_device_transaction" });
      }

      if (now() >= item.expiresAt) {
        deviceLogins.delete(id);
        return send(res, 410, { error: "device_code_expired" });
      }

      if (now() < item.nextPollAt) {
        return send(res, 202, {
          pending: true,
          retry_after: Math.ceil((item.nextPollAt - now()) / 1000)
        });
      }

      item.nextPollAt = now() + item.intervalMs;
      const { response, body } = await exchangeDeviceCode(item.deviceCode);

      if (!response.ok) {
        if (body.error === "authorization_pending") {
          return send(res, 202, {
            pending: true,
            retry_after: Math.ceil(item.intervalMs / 1000)
          });
        }
        if (body.error === "slow_down") {
          item.intervalMs += 5000;
          item.nextPollAt = now() + item.intervalMs;
          return send(res, 202, {
            pending: true,
            retry_after: Math.ceil(item.intervalMs / 1000)
          });
        }

        deviceLogins.delete(id);
        return send(res, 502, {
          error: "device_token_exchange_failed",
          discord_status: response.status,
          discord: body
        });
      }

      if (!body.access_token) {
        return send(res, 502, { error: "missing_access_token" });
      }

      const me = await discordJson("/users/@me", body.access_token);
      if (!me.ok) {
        deviceLogins.delete(id);
        return send(res, 502, {
          error: "identity_failed",
          discord_status: me.status
        });
      }

      const rawSession = randomToken();
      const sessionKey = hashToken(rawSession);

      sessions.set(sessionKey, {
        user: me.body,
        accessToken: body.access_token,
        refreshToken: body.refresh_token || "",
        accessExpiresAt: now() + Number(body.expires_in || 604800) * 1000,
        sessionExpiresAt: now() + APP_SESSION_TTL_MS,
        createdAt: now()
      });
      saveSessions();
      deviceLogins.delete(id);

      return send(res, 200, {
        authenticated: true,
        session: rawSession,
        user: me.body
      });
    }

    const auth = await authenticate(req);
    if (!auth) return send(res, 401, { error: "unauthorized" });

    const { key, session } = auth;

    if (req.method === "GET" && url.pathname === "/session") {
      return send(res, 200, {
        authenticated: true,
        user: session.user,
        session_expires_at: session.sessionExpiresAt
      });
    }

    if (req.method === "POST" && url.pathname === "/oauth/logout") {
      sessions.delete(key);
      saveSessions();
      return send(res, 200, { ok: true });
    }

    if (req.method === "GET" && url.pathname === "/me") {
      return send(res, 200, session.user);
    }

    if (req.method === "GET" && url.pathname === "/guilds") {
      const result = await discordJson("/users/@me/guilds", session.accessToken);
      if (!result.ok) return send(res, result.status, result.body);

      const guilds = Array.isArray(result.body)
        ? result.body.slice(0, 48).map((guild) => ({
            id: guild.id,
            name: guild.name || "Discord server",
            icon: guild.icon || "",
            owner: Boolean(guild.owner)
          }))
        : [];

      return send(res, 200, guilds);
    }

    const iconMatch = url.pathname.match(/^\/guilds\/(\d+)\/icon$/);
    if (req.method === "GET" && iconMatch) {
      const guildId = iconMatch[1];
      const iconHash = url.searchParams.get("hash") || "";

      if (!/^[A-Za-z0-9_]+$/.test(iconHash)) {
        return send(res, 400, { error: "invalid_icon_hash" });
      }

      /*
       * Fixed Discord CDN origin avoids arbitrary proxying/SSRF.
       * PNG is requested even for animated hashes because Vita2D's
       * loader supports PNG/JPEG but not Discord's animated GIF path.
       */
      const cdnUrl =
        `https://cdn.discordapp.com/icons/${guildId}/${iconHash}.png?size=128`;

      const response = await fetch(cdnUrl, {
        headers: { "user-agent": "Discord Vita/1.7" }
      });

      if (!response.ok) {
        return send(res, response.status, { error: "icon_not_available" });
      }

      const bytes = Buffer.from(await response.arrayBuffer());
      if (bytes.length > 512 * 1024) {
        return send(res, 413, { error: "icon_too_large" });
      }

      res.writeHead(200, {
        "content-type": "image/png",
        "content-length": String(bytes.length),
        "cache-control": "private, max-age=86400",
        "x-content-type-options": "nosniff"
      });
      return res.end(bytes);
    }

    let route = url.pathname.match(/^\/guilds\/(\d+)\/channels$/);
    if (req.method === "GET" && route) {
      const guildId = route[1];
      const ctx = await loadGuildAccessContext(session.accessToken, guildId);
      if (!ctx.ok) return send(res, ctx.status || 403, ctx.body);

      const channels = (Array.isArray(ctx.channels) ? ctx.channels : [])
        .filter((channel) => channel.type === 0 || channel.type === 5)
        .filter((channel) => {
          const permissions = computeChannelPermissions({
            guildId: ctx.guild.id,
            ownerId: ctx.guild.owner_id,
            member: ctx.member,
            roles: ctx.roles,
            channel
          });
          return (permissions & PERM_VIEW_CHANNEL) !== 0n;
        })
        .sort((a, b) => (a.position ?? 0) - (b.position ?? 0))
        .slice(0, 48)
        .map((channel) => ({
          id: channel.id,
          name: channel.name || "channel",
          type: channel.type,
          position: channel.position ?? 0
        }));

      return send(res, 200, channels);
    }

    route = url.pathname.match(/^\/channels\/(\d+)\/messages$/);
    if (route) {
      const channelId = route[1];
      const access = await resolveBotChannelForUser(
        session.accessToken,
        channelId
      );

      if (!access.ok) {
        return send(res, access.status || 403, access.body || {
          error: "channel_not_available"
        });
      }

      if (req.method === "GET") {
        const required = PERM_VIEW_CHANNEL | PERM_READ_MESSAGE_HISTORY;
        if ((access.permissions & required) !== required) {
          return send(res, 403, { error: "user_cannot_read_channel" });
        }

        const result = await discordBot(
          `/channels/${channelId}/messages?limit=24`
        );

        if (!result.ok) return send(res, result.status, result.body);

        const messages = Array.isArray(result.body)
          ? result.body.map((message) => ({
              id: message.id,
              content: message.content || "",
              timestamp: message.timestamp || "",
              author: {
                id: message.author?.id || "",
                username:
                  message.author?.global_name ||
                  message.author?.username ||
                  "Discord user",
                bot: Boolean(message.author?.bot)
              }
            }))
          : [];

        return send(res, 200, messages);
      }

      if (req.method === "POST") {
        const required = PERM_VIEW_CHANNEL | PERM_SEND_MESSAGES;
        if ((access.permissions & required) !== required) {
          return send(res, 403, { error: "user_cannot_send_to_channel" });
        }

        let raw = "";
        let bytes = 0;

        for await (const chunk of req) {
          bytes += chunk.length;
          if (bytes > 16 * 1024) {
            return send(res, 413, { error: "request_too_large" });
          }
          raw += chunk;
        }

        let body;
        try {
          body = JSON.parse(raw || "{}");
        } catch {
          return send(res, 400, { error: "invalid_json" });
        }

        if (
          typeof body.content !== "string" ||
          !body.content.trim() ||
          body.content.length > 2000
        ) {
          return send(res, 400, { error: "invalid_content" });
        }

        const result = await discordBot(
          `/channels/${channelId}/messages`,
          {
            method: "POST",
            headers: { "content-type": "application/json" },
            body: JSON.stringify({
              content: body.content,
              allowed_mentions: { parse: [] }
            })
          }
        );

        return send(res, result.status, result.body);
      }
    }

    return send(res, 404, { error: "not_found" });
  } catch (error) {
    console.error(error);
    return send(res, 500, { error: "internal_error" });
  }
}).listen(PORT, () => {
  console.log(`Discord Vita backend listening on ${PORT}`);
});
