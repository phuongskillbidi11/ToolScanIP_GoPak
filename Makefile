# ── IP Scanner — Makefile ──────────────────────────────────────────────────
#
#  make              → native build (x86_64, WSL2/Ubuntu)
#  make pi           → cross-compile for Raspberry Pi (arm-linux-gnueabihf)
#  make aarch64      → cross-compile for Orange Pi isoftembedded (aarch64)
#  make deploy       → deploy to Pi #1  (admin@100.66.44.107, eth0)
#  make deploy2      → deploy to Pi #2  (intercom@100.70.31.100 , wlan0) / pass : admin@123
#  make deploy3      → deploy to Orange Pi (root@100.101.117.23, lan0)
#  make clean        → remove all build artifacts
#  sudo make install → install native binary to /usr/local/bin

# ── Raspberry Pi #1 — isoft  (eth0,  192.168.20.x, Tailscale 100.66.44.107)
PI_HOST   = admin@100.66.44.107
PI_DIR    = ~/ToolScanIP
PI_CC     = arm-linux-gnueabihf-gcc

# ── Raspberry Pi #2 — intercom  (wlan0, 192.168.3.x)
PI2_HOST  = intercom@100.70.31.100
PI2_DIR   = ~/ToolScanIP
PI2_PASS  = admin@123
PI2_SSH   = sshpass -p '$(PI2_PASS)' ssh  -o StrictHostKeyChecking=no
PI2_SCP   = sshpass -p '$(PI2_PASS)' scp  -o StrictHostKeyChecking=no

# ── Orange Pi — isoftembedded  (lan0, 192.168.41.x, Tailscale 100.101.117.23)
PI3_HOST  = root@100.101.117.23
PI3_DIR   = ~/ToolScanIP
PI3_CC    = aarch64-linux-gnu-gcc

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

# ── Inline web/src/ sources into web/scanner.html, then embed ─────────────
WEB_SRC = $(wildcard web/src/assets/css/*.css web/src/assets/js/*.js web/src/scanner.html)

web/scanner.html: $(WEB_SRC) web/build.py
	python3 web/build.py

src/scanner_html.h: web/scanner.html
	xxd -i $< | sed 's/web_scanner_html/scanner_html/g' > $@

# ── Native build (x86_64) ──────────────────────────────────────────────────
TARGET   = ipscanner
OBJS     = $(SRCS:.c=.o)

.PHONY: all pi aarch64 deploy deploy2 deploy3 native3 build2 clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  [native] Build successful: ./$(TARGET)"
	@echo "  Run with:  sudo ./$(TARGET) -i eth0"
	@echo ""

src/web.o: src/scanner_html.h
src/web.pi.o: src/scanner_html.h
src/web.aarch64.o: src/scanner_html.h

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

# ── Orange Pi cross-compile (aarch64-linux-gnu) ────────────────────────────
# Link pthread statically: Ubuntu 22.04 toolchain targets GLIBC 2.34 where
# pthread was merged into libc.so.6, but the Orange Pi only has GLIBC 2.31.
A64_TARGET  = ipscanner-aarch64
A64_OBJS    = $(SRCS:.c=.aarch64.o)
A64_LDFLAGS = -Wl,-Bstatic -lpthread -Wl,-Bdynamic

aarch64: $(A64_TARGET)

$(A64_TARGET): $(A64_OBJS)
	$(PI3_CC) $(CFLAGS) -o $@ $^ $(A64_LDFLAGS)
	@echo ""
	@echo "  [aarch64] Cross-compile successful: ./$(A64_TARGET)"
	@echo "  Deploy with:  make deploy3"
	@echo ""

%.aarch64.o: %.c
	$(PI3_CC) $(CFLAGS) -c -o $@ $<

# ── Deploy to Pi via scp ───────────────────────────────────────────────────
deploy: $(PI_TARGET)
	@echo "  Uploading to $(PI_HOST):$(PI_DIR) ..."
	ssh $(PI_HOST) "mkdir -p $(PI_DIR)"
	scp $(PI_TARGET)         $(PI_HOST):/tmp/ipscanner_new
	ssh $(PI_HOST) "mv /tmp/ipscanner_new $(PI_DIR)/ipscanner && chmod +x $(PI_DIR)/ipscanner"
	ssh $(PI_HOST) "mkdir -p $(PI_DIR)/scripts"
	scp scripts/gen-nginx.sh              $(PI_HOST):$(PI_DIR)/scripts/gen-nginx.sh
	scp scripts/fetch-machine-names.sh    $(PI_HOST):$(PI_DIR)/scripts/fetch-machine-names.sh
	scp scripts/mqtt-sync-comments.sh     $(PI_HOST):$(PI_DIR)/scripts/mqtt-sync-comments.sh
	ssh $(PI_HOST) "chmod +x $(PI_DIR)/scripts/*.sh && ln -sf $(PI_DIR)/scripts/gen-nginx.sh $(PI_DIR)/gen-nginx.sh"
	@echo ""
	@echo "  Done. On the Pi:"
	@echo "    cd $(PI_DIR) && sudo ./scripts/fetch-machine-names.sh"
	@echo "    cd $(PI_DIR) && sudo ./gen-nginx.sh"
	@echo ""

# ── Deploy to Pi #2 (intercom — wlan0) ────────────────────────────────────
deploy2: $(PI_TARGET)
	@echo "  Uploading to $(PI2_HOST):$(PI2_DIR) ..."
	$(PI2_SSH) $(PI2_HOST) "mkdir -p $(PI2_DIR)"
	$(PI2_SCP) $(PI_TARGET)          $(PI2_HOST):/tmp/ipscanner_new
	$(PI2_SSH) $(PI2_HOST) "mv /tmp/ipscanner_new $(PI2_DIR)/ipscanner && chmod +x $(PI2_DIR)/ipscanner"
	$(PI2_SSH) $(PI2_HOST) "mkdir -p $(PI2_DIR)/scripts"
	$(PI2_SCP) scripts/gen-nginx.sh              $(PI2_HOST):$(PI2_DIR)/scripts/gen-nginx.sh
	$(PI2_SCP) scripts/fetch-machine-names.sh    $(PI2_HOST):$(PI2_DIR)/scripts/fetch-machine-names.sh
	$(PI2_SCP) scripts/mqtt-sync-comments.sh     $(PI2_HOST):$(PI2_DIR)/scripts/mqtt-sync-comments.sh
	$(PI2_SSH) $(PI2_HOST) "chmod +x $(PI2_DIR)/scripts/*.sh && ln -sf $(PI2_DIR)/scripts/gen-nginx.sh $(PI2_DIR)/gen-nginx.sh"
	@echo ""
	@echo "  Done. On Pi #2 (intercom/wlan0):"
	@echo "    cd $(PI2_DIR) && sudo ./gen-nginx.sh wlan0"
	@echo ""

# ── Build + deploy Pi #2 in one command ────────────────────────────────────
build2: pi deploy2

# ── Native compile + deploy to Orange Pi (avoids GLIBC cross-compile mismatch)
# Uses the board's own gcc 10.2.1 which links against its GLIBC 2.31.
native3: src/scanner_html.h
	@echo "  Copying sources to $(PI3_HOST):$(PI3_DIR) ..."
	ssh $(PI3_HOST) "mkdir -p $(PI3_DIR)/src $(PI3_DIR)/scripts"
	scp src/*.c src/*.h                       $(PI3_HOST):$(PI3_DIR)/src/
	scp scripts/gen-nginx.sh                  $(PI3_HOST):$(PI3_DIR)/scripts/gen-nginx.sh
	scp scripts/fetch-machine-names.sh        $(PI3_HOST):$(PI3_DIR)/scripts/fetch-machine-names.sh
	scp scripts/mqtt-sync-comments.sh         $(PI3_HOST):$(PI3_DIR)/scripts/mqtt-sync-comments.sh
	@echo "  Compiling natively on board ..."
	ssh $(PI3_HOST) "cd $(PI3_DIR) && \
	    gcc $(CFLAGS) \
	        src/main.c src/arp.c src/oui.c src/probe.c src/comments.c \
	        src/scanner.c src/display.c src/web.c \
	        -o ipscanner $(LDFLAGS) && \
	    chmod +x ipscanner && \
	    chmod +x scripts/*.sh && \
	    ln -sf $(PI3_DIR)/scripts/gen-nginx.sh $(PI3_DIR)/gen-nginx.sh && \
	    echo '[native3] Build on board successful'"
	@echo ""
	@echo "  Done. On Orange Pi:"
	@echo "    cd $(PI3_DIR) && sudo ./ipscanner -i lan0"
	@echo "    cd $(PI3_DIR) && sudo ./gen-nginx.sh lan0"
	@echo ""

# ── Deploy to Orange Pi (root@100.101.117.23 — lan0) ──────────────────────
deploy3: $(A64_TARGET)
	@echo "  Uploading to $(PI3_HOST):$(PI3_DIR) ..."
	ssh $(PI3_HOST) "mkdir -p $(PI3_DIR)"
	scp $(A64_TARGET)             $(PI3_HOST):/tmp/ipscanner_new
	ssh $(PI3_HOST) "mv /tmp/ipscanner_new $(PI3_DIR)/ipscanner && chmod +x $(PI3_DIR)/ipscanner"
	ssh $(PI3_HOST) "mkdir -p $(PI3_DIR)/scripts"
	scp scripts/gen-nginx.sh              $(PI3_HOST):$(PI3_DIR)/scripts/gen-nginx.sh
	scp scripts/fetch-machine-names.sh    $(PI3_HOST):$(PI3_DIR)/scripts/fetch-machine-names.sh
	scp scripts/mqtt-sync-comments.sh     $(PI3_HOST):$(PI3_DIR)/scripts/mqtt-sync-comments.sh
	ssh $(PI3_HOST) "chmod +x $(PI3_DIR)/scripts/*.sh && ln -sf $(PI3_DIR)/scripts/gen-nginx.sh $(PI3_DIR)/gen-nginx.sh"
	@echo ""
	@echo "  Done. On Orange Pi (isoftembedded/lan0):"
	@echo "    cd $(PI3_DIR) && sudo ./ipscanner -i lan0"
	@echo "    cd $(PI3_DIR) && sudo ./gen-nginx.sh lan0"
	@echo ""

# ── Clean ──────────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(PI_OBJS) $(A64_OBJS) $(TARGET) $(PI_TARGET) $(A64_TARGET)

# ── Install native binary ──────────────────────────────────────────────────
install: $(TARGET)
	install -m 4755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "Installed to /usr/local/bin/$(TARGET) (setuid root)"