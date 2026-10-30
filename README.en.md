# InkRSS

RSS feeds on PocketBook e-readers: a morning issue you turn with the page buttons, like a book. Full article text and images come through your own [Miniflux](https://miniflux.app) server; after a refresh everything reads offline.

*[Русская версия](README.md) · Site: [0rb1ta.github.io/InkRSS](https://0rb1ta.github.io/InkRSS/)*

For now **Miniflux** (self-hosted or hosted) is the only supported server. Without a server, feeds can still be read directly from the sites, but without full text.

## Features

- connects to Miniflux with a login and password or an API key — on the screen or with a `config.json` file;
- the issue: a lead article, sections by folder or feed, the feed and reading time next to every headline;
- full text: for short articles the server fetches the page from the site;
- offline reading: texts and images (in greys, scaled to the screen) download when you tap Refresh;
- read and starred marks sync with Miniflux; marks made offline are queued and sent with the next refresh;
- "Mark all as read" for the issue, a feed and a folder;
- articles in pages, turned with the buttons and swipes; font (PT Serif, Literata, Roboto and the reader's own fonts) and size right in the menu;
- a QR code of the original link, to open it on a phone;
- without a server: add a feed by address, import OPML;
- Russian and English interface.

<a id="install"></a>
## Installation

Tested on PocketBook 970 (InkPad Lite), firmware 6.8. On other models with firmware 6.x it will most likely run, but this is not tested — please tell us in [Issues](https://github.com/0rb1ta/InkRSS/issues) whether it works: model, firmware version and, if something is wrong, the file `system/inkrss/log.txt`.

1. Connect the reader to a computer over USB.
2. Copy the contents of the release archive to the root of the internal storage: `applications/InkRSS.app`, `applications/icons/` and the folder `system/inkrss/`.
3. Disconnect the reader and start InkRSS from Applications.

### Updates

After the first successful refresh the app checks GitHub for a new release. If there is one, an arrow icon appears in the header of the main screen; tapping it shows a QR code with the release page. A new version is copied the same way, over the old files: the connection and downloaded articles stay.

To uninstall, delete `applications/InkRSS.app`, the files `applications/icons/inkrss*.bmp` and the folder `system/inkrss/`. If the icon was added to the Applications menu, remove it in the settings first.

### Connecting to Miniflux

Enter the server address and your login and password (or an API key: Miniflux → Settings → API Keys). To avoid typing on the on-screen keyboard, put a file `system/inkrss/config.json` on the reader:

```json
{ "mode": "miniflux", "server": "https://rss.example.org", "token": "API key" }
```

Full text works best when it is turned on in Miniflux itself: "Fetch original content" in the feed settings. For short articles InkRSS asks the server to do it.

### Without a server

Tap "No server: feeds directly". Add feeds by address (Feeds and folders → "+") or from an `.opml` file in the root of the reader's storage.

## Known limitations

- Miniflux is the only server. Self-signed certificates are not supported.
- The firmware turns Wi-Fi off on its own: the app turns it on before a refresh, which takes a few seconds.
- Refresh only on demand, up to 200 new articles at a time (changeable in the settings).

## Building

You need the PocketBook SDK B288 6.8 (`github.com/pocketbook/SDK_6.3.0`, release 6.8).

```bash
app/package/build-device.sh
```

## License

InkRSS is distributed under the GNU GPL v3 (see [LICENSE](LICENSE)).
The program comes with no warranty (see the license); you install it at your own risk.
The app links dynamically against the PocketBook firmware system libraries (`libinkview`, `libcommon_utilities`); they are not part of InkRSS and are not distributed with it.
Third-party components (Font Awesome icons, PT Serif and Literata fonts, the QR code library, Roboto letter outlines in the logo) and their licenses: [app/third_party/LICENSES.md](app/third_party/LICENSES.md).
