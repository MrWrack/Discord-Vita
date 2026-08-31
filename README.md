# Discord Vita v1.7

Discord Vita v1.7 gör server → kanal → meddelanden → skicka fungerande via en
officiellt stödd Discord-bot på backend.

## Vad fungerar

- Discord OAuth device-login
- Auto-login/session
- Riktig serverlista för den inloggade användaren
- Riktiga serverikoner
- Riktiga textkanaler för servrar där Discord Vita-boten finns
- Läsa meddelanden som boten har rätt att läsa
- PS Vita IME/systemtangentbord
- Skicka meddelanden från Vita
- Touch + D-pad
- Lokal ikon-cache
- Explicit logout

## Viktigt: vem skickar meddelandet?

Discords vanliga OAuth `guilds`-scope ger en användare serverlistan, men är inte ett
generellt scope för att bygga en alternativ Discord-user-klient. `messages.read` gäller
lokal RPC och är inte ett fristående user-token-scope för detta användningsfall.

Därför använder v1.5 en Discord-bot för kanal/message REST API.

**Meddelanden som skickas från Vita visas som Discord Vita-boten, inte som ditt privata konto.**

Det här är avsiktligt. Projektet använder inte self-bot eller en vanlig Discord-user-token
för automatiserade kanal/message-anrop.

## Backend-konfiguration

```sh
cd server
cp .env.example .env
npm install
npm start
```

`.env`:

```env
DISCORD_CLIENT_ID=YOUR_CLIENT_ID
DISCORD_CLIENT_SECRET=YOUR_CLIENT_SECRET
DISCORD_BOT_TOKEN=YOUR_BOT_TOKEN
PORT=8080
SESSION_STORE=./sessions.json
DISCORD_DEVICE_SCOPE=identify guilds guilds.members.read
APP_SESSION_TTL_DAYS=30
```

Kör backend bakom HTTPS.

## Lägg till boten i servern

Discord Vita-boten behöver finnas i varje server där Vita ska kunna öppna chatten.

Minsta relevanta kanalrättigheter är:

- View Channel
- Read Message History
- Send Messages

Ge bara boten de rättigheter den behöver.

Backend kontrollerar dessutom att den OAuth-inloggade användaren själv tillhör guilden
innan botens kanal/message-endpoints används.

## Vita

Ändra:

`vita/include/config.h`

```c
#define DISCORD_VITA_BACKEND "https://din-backend.example"
```

Bygg med VitaSDK + libvita2d:

```sh
cd vita
mkdir build
cd build
cmake ..
make
```

## Kontroller

Hem:
- Upp/Ner: server
- X: öppna kanaler
- Triangle: serverinfo
- START: konto
- Circle: avsluta men behåll session
- Touch: välj server

Kanaler:
- Upp/Ner: kanal
- X: öppna chat
- Circle: tillbaka
- Touch: välj kanal

Chat:
- Square: öppna PS Vita-systemtangentbordet
- Triangle: uppdatera meddelanden
- Circle: tillbaka

## Säkerhet

- Discord Client Secret finns endast på backend.
- Bot Token finns endast på backend.
- Vita lagrar endast Discord Vita-appsessionen.
- Backend hash-lagrar appsessionens bearer-token som sessionsnyckel.
- Kanal-ID valideras via bot-API och användarens guild-medlemskap innan meddelanden läses/skickas.
- `allowed_mentions.parse` är tomt när boten skickar, för att undvika oväntade mass-mentions.

## Byggstatus

`server.js` syntaxkontrolleras med Node när paketet skapas.
VitaSDK-kompilatorn finns inte installerad i den här arbetsmiljön, så ett riktigt
`arm-vita-eabi-gcc`/VPK-test måste fortfarande göras i en VitaSDK-miljö.


## v1.6 compile-fix

v1.5 hade ett faktiskt syntaxfel i `vita/src/main.c` efter en automatisk kodersättning.
v1.6 skriver om state-maskinen rent och tar bort de trasiga `if dv_input_button...`-raderna
samt flyttar `touch_poll` till rätt scope.

### Bygg med Docker

Med Docker installerat:

```sh
./build-vpk-docker.sh
```

Scriptet använder den officiella VitaSDK-imagen `vitasdk/vitasdk:nightly`.
Resultatet blir `Discord-Vita-v1.7.vpk` i projektroten.

Det finns även `.github/workflows/vita-build.yml` som bygger VPK automatiskt i GitHub Actions.


## v1.7-fixar

- OAuth-polling använder nu `sceKernelGetProcessTimeWide()` i stället för bildrutor.
- IME-modulen laddas/avladdas explicit med `SCE_SYSMODULE_IME`.
- Nätverksinit städar korrekt vid partiella fel.
- `/guilds` returnerar en kompakt Vita-anpassad lista för att minska risken för truncering.
- Kanal- och message-endpoints kontrollerar den inloggade användarens egna Discord-rättigheter.
- `guilds.members.read` används för att hämta användarens guild-medlemskap/roller.
- Discords roll- och channel-overwrite-ordning räknas server-side innan en kanal visas/läses/skrivs.
- Unicode-statussymboler som kan saknas i Vita PGF-fonten har ersatts med ASCII.
- Docker/CI använder `vitasdk/vitasdk:latest`.

### Viktigt efter uppgradering

Gamla OAuth-sessioner från v1.6 saknar normalt `guilds.members.read`.
Logga därför ut och in igen efter uppgradering till v1.7.
