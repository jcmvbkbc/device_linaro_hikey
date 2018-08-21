CC := xt-xcc --xtensa-core=hifi3_hikey960
LD := $(CC)

srcdir := $(PWD)
builddir := $(srcdir)/build

CPPFLAGS := -I$(builddir)/include
CFLAGS := -mlongcalls -mtext-section-literals
LDFLAGS := -mlsp=$(srcdir)/xrp/xrp-example/hikey960 \
	-Wl,--defsym,_memmap_cacheattr_reset=0x24222222 \
	-Wl,--defsym,xrp_dsp_comm_base_magic=0x20161006

LIBS := $(builddir)/lib/libxrp-dsp.a \
	$(builddir)/lib/libxrp-dsp-hw-hikey960.a \
	$(builddir)/lib/libxrp-common.a
OBJS := dsp_main.o

all: xrp0.elf
clean:
	rm -rf xrp0.elf $(OBJS) $(builddir)

xrp0.elf: $(OBJS)
	$(LD) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

$(OBJS): $(builddir)/xrp/.built

$(builddir)/xrp/.built:
	mkdir -p $(builddir)/xrp ; \
	cd $(builddir)/xrp && \
	$(srcdir)/xrp/configure --prefix=$(builddir) \
				--host=xtensa-elf \
				--enable-dsp \
				--enable-port=hikey960 \
				CFLAGS="$(CFLAGS)" \
				CC="$(CC)" && \
	make && make install && touch $@
