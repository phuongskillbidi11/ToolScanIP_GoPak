# ── IP Scanner — Makefile ──────────────────────────────────────────────────
#
#  make              → native build (x86_64, WSL2/Ubuntu)
#  make pi           → cross-compile for Raspberry Pi (arm-linux-gnueabihf)
#  make deploy       → cross-compile + scp to Pi
#  make clean        → remove all build artifacts
#  sudo make install → install native binary to /usr/local/bin

# ── Raspberry Pi target config ─────────────────────────────────────────────
PI_HOST   = admin@100.66.44.107
PI_DIR    = ~/ToolScanIP
PI_CC     = arm-linux-gnueabihf-gcc

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

# ── Native build (x86_64) ──────────────────────────────────────────────────
TARGET   = ipscanner
OBJS     = $(SRCS:.c=.o)

.PHONY: all pi deploy clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  [native] Build successful: ./$(TARGET)"
	@echo "  Run with:  sudo ./$(TARGET) -i eth0"
	@echo ""

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
	scp scripts/gen-nginx.sh $(PI_HOST):$(PI_DIR)/gen-nginx.sh
	ssh $(PI_HOST) "chmod +x $(PI_DIR)/gen-nginx.sh"
	@echo ""
	@echo "  Done. On the Pi:"
	@echo "    sudo apt install -y nginx"
	@echo "    cd $(PI_DIR) && sudo ./gen-nginx.sh"
	@echo ""

# ── Clean ──────────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(PI_OBJS) $(TARGET) $(PI_TARGET)

# ── Install native binary ──────────────────────────────────────────────────
install: $(TARGET)
	install -m 4755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "Installed to /usr/local/bin/$(TARGET) (setuid root)"