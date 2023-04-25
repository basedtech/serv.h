# Desktop Web App Library
- This is a "more practical" replacement for frameworks like Electron, Webview, and Tauri
- Instead of opening a new browser instance (which uses a lot of RAM), this library starts a HTTP web
server that allows communication with a C backend.
- The user is given a link to open in their favorite browser.

Currently, this library is in development and not intended for use in software yet.

## Pros over webview/tauri/etc:
- 1.6mb static hello world binary (GCC, glib, staticx)
(GTK3.0 Hello World is 9mb)
- No unneccessary RAM usage (does not load up a web engine)
- Easy to deploy into production
- Works well for large data transfers between backend and frontend
- Single header C99, easy to compile and work with
- It's possible for an app to use no *no* Javascript (work in text browers even)

## Cons
- Not intuitive, confusing to users
- Feels slightly janky (still less janky than 60-100mb Hello World binaries)

### Future plans
- [x] Support Linux
- [x] Support Windows
- [ ] Support MacOS
- [ ] Implement better/faster bindings
- [ ] Parse POST requests
- [x] Open a small window when running the app with instructions

Copyright (C) by Based Technologies - Published under MIT License
