# Settings shared by the device app and the host tests.
CONFIG += c++17
VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
# Build time, written to the log at start: shows at once whether the reader runs the latest build.
APP_BUILD = $$system(date '+%Y-%m-%d_%H:%M')
DEFINES += APP_BUILD=\\\"$$APP_BUILD\\\"
