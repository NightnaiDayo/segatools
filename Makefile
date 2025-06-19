V               ?= @

.DEFAULT_GOAL := help

BUILD_DIR := build
BUILD_DIR_32 := $(BUILD_DIR)/build32
BUILD_DIR_64 := $(BUILD_DIR)/build64
BUILD_DIR_GAMES_32 := $(BUILD_DIR_32)/games
BUILD_DIR_GAMES_64 := $(BUILD_DIR_64)/games
BUILD_DIR_ZIP := $(BUILD_DIR)/zip

DOC_DIR := doc

DIST_DIR := dist

# Add "-D[option]=[value]" here as necessary
MESON_OPTIONS :=
# For options that shouldn't be committed
-include MesonLocalOptions.mk

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------

include Package.mk

.PHONY: build # Build the project
build:
	$(V)meson setup $(MESON_OPTIONS) --cross cross-mingw-32.txt $(BUILD_DIR_32)
	$(V)ninja -C $(BUILD_DIR_32)
	$(V)meson setup $(MESON_OPTIONS) --cross cross-mingw-64.txt $(BUILD_DIR_64)
	$(V)ninja -C $(BUILD_DIR_64)

.PHONY: dist # Build and create a zip distribution package
dist: build clean-zip zip

.PHONY: clean-zip # Remove zip files from build dir before packaging
clean-zip:
	$(V)rm -Rf $(BUILD_DIR_ZIP)/*.zip

.PHONY: zip # Create a zip distribution pacakge
zip: $(BUILD_DIR_ZIP)/segatools.zip

.PHONY: clean # Cleanup build output
clean:
	$(V)rm -rf $(BUILD_DIR) subprojects/capnhook

# -----------------------------------------------------------------------------
# Utility, combo and alias targets
# -----------------------------------------------------------------------------

# Help screen note:
# Variables that need to be displayed in the help screen need to strictly
# follow the pattern "^[A-Z_]+ \?= .* # .*".
# Targets that need to be displayed in the help screen need to add a separate
# phony definition strictly following the pattern "^\.PHONY\: .* # .*".

.PHONY: help # Print help screen
help:
	$(V)echo segatools makefile.
	$(V)echo
	$(V)echo "Environment variables:"
	$(V)grep -E '^[A-Z_]+ \?\= .* #' Makefile | gawk 'match($$0, /([A-Z_]+) \?= [$$\(]*([^\)]*)[\)]{0,1} # (.*)/, a) { printf("  \033[0;35m%-25s \033[0;0m%-45s [%s]\n", a[1], a[3], a[2]) }'
	$(V)echo ""
	$(V)echo "Targets:"
	$(V)grep '^.PHONY: .* #' Makefile | gawk 'match($$0, /\.PHONY: (.*) # (.*)/, a) { printf("  \033[0;32m%-25s \033[0;0m%s\n", a[1], a[2]) }'
