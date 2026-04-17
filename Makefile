# ── IP Scanner — Makefile ──────────────────────────────────────────────────
#
#  make              → native build (x86_64, WSL2/Ubuntu)
#  make pi           → cross-compile for Raspberry Pi (arm-linux-gnueabihf)
#  make deploy       → deploy to Pi #1  (admin@100.66.44.107, eth0)
#  make deploy2      → deploy to Pi #2  (intercom@192.168.3.155, wlan0)
#  make clean        → remove all build artifacts
#  sudo make install → install native binary to /usr/local/bin

# ── Raspberry Pi #1 — isoft  (eth0,  192.168.20.x, Tailscale 100.66.44.107)
PI_HOST   = admin@100.66.44.107
PI_DIR    = ~/ToolScanIP
PI_CC     = arm-linux-gnueabihf-gcc

# ── Raspberry Pi #2 — intercom  (wlan0, 192.168.3.x)
PI2_HOST  = intercom@192.168.3.155
PI2_DIR   = ~/ToolScanIP

# ── Common sources ─────────────────────────────────────────────────────────
SRCDIR  = src
SRCS    = $(SRCDIR)/main.c      \
          $(SRCDIR)/arp.c       \
          $(SRCDIR)/oui.c       \
          $(SRCDIR)/probe.c     \
          $(SRCDIR)/comments.c  \
          $(SRCDIR)/scanner.c   \
          $(SRCDIR)/display.c   \
          $(SRCDIR)/web.c

CFLAGS  = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
LDFLAGS = -lpthread

# ── Embedded HTML (auto-generated from web/scanner.html) ──────────────────
src/scanner_html.h: web/scanner.html
	xxd -i $< | sed 's/web_scanner_html/scanner_html/g' > $@

# ── Native build (x86_64) ──────────────────────────────────────────────────
TARGET   = ipscanner
OBJS     = $(SRCS:.c=.o)

.PHONY: all pi deploy deploy2 clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  [native] Build successful: ./$(TARGET)"
	@echo "  Run with:  sudo ./$(TARGET) -i eth0"
	@echo ""

src/web.o: src/scanner_html.h
src/web.pi.o: src/scanner_html.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Raspberry Pi cross-compile (arm-linux-gnueabihf) ──────────────────────
PI_TARGET = ipscanner-pi
PI_OBJS   = $(SRCS:.c=.pi.o)

pi: $(PI_TARGET)

$(PI_TARGET): $(PI_OBJS)
	$(PI_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  [pi] Cross-compile successful: ./$(PI_TARGET)"
	@echo "  Deploy with:  make deploy"
	@echo ""

%.pi.o: %.c
	$(PI_CC) $(CFLAGS) -c -o $@ $<

# ── Deploy to Pi via scp ───────────────────────────────────────────────────
deploy: $(PI_TARGET)
	@echo "  Uploading to $(PI_HOST):$(PI_DIR) ..."
	ssh $(PI_HOST) "mkdir -p $(PI_DIR)"
	scp $(PI_TARGET)         $(PI_HOST):/tmp/ipscanner_new
	ssh $(PI_HOST) "mv /tmp/ipscanner_new $(PI_DIR)/ipscanner && chmod +x $(PI_DIR)/ipscanner"
	ssh $(PI_HOST) "mkdir -p $(PI_DIR)/scripts"
	scp scripts/gen-nginx.sh          $(PI_HOST):$(PI_DIR)/scripts/gen-nginx.sh
	scp scripts/fetch-machine-names.sh $(PI_HOST):$(PI_DIR)/scripts/fetch-machine-names.sh
	ssh $(PI_HOST) "chmod +x $(PI_DIR)/scripts/*.sh && ln -sf $(PI_DIR)/scripts/gen-nginx.sh $(PI_DIR)/gen-nginx.sh"
	@echo ""
	@echo "  Done. On the Pi:"
	@echo "    cd $(PI_DIR) && sudo ./scripts/fetch-machine-names.sh"
	@echo "    cd $(PI_DIR) && sudo ./gen-nginx.sh"
	@echo ""

# ── Deploy to Pi #2 (intercom — wlan0) ────────────────────────────────────
deploy2: $(PI_TARGET)
	@echo "  Uploading to $(PI2_HOST):$(PI2_DIR) ..."
	ssh $(PI2_HOST) "mkdir -p $(PI2_DIR)"
	scp $(PI_TARGET)          $(PI2_HOST):/tmp/ipscanner_new
	ssh $(PI2_HOST) "mv /tmp/ipscanner_new $(PI2_DIR)/ipscanner && chmod +x $(PI2_DIR)/ipscanner"
	ssh $(PI2_HOST) "mkdir -p $(PI2_DIR)/scripts"
	scp scripts/gen-nginx.sh           $(PI2_HOST):$(PI2_DIR)/scripts/gen-nginx.sh
	scp scripts/fetch-machine-names.sh $(PI2_HOST):$(PI2_DIR)/scripts/fetch-machine-names.sh
	ssh $(PI2_HOST) "chmod +x $(PI2_DIR)/scripts/*.sh && ln -sf $(PI2_DIR)/scripts/gen-nginx.sh $(PI2_DIR)/gen-nginx.sh"
	@echo ""
	@echo "  Done. On Pi #2 (intercom/wlan0):"
	@echo "    cd $(PI2_DIR) && sudo apt install wireshark-common"
	@echo "    cd $(PI2_DIR) && sudo ./gen-nginx.sh wlan0"
	@echo ""

# ── Clean ──────────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(PI_OBJS) $(TARGET) $(PI_TARGET)

# ── Install native binary ──────────────────────────────────────────────────
install: $(TARGET)
	install -m 4755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "Installed to /usr/local/bin/$(TARGET) (setuid root)"