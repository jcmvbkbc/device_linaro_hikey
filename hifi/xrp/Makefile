CC := xt-xcc
LD := xt-xcc

srcdir := $(PWD)
builddir := $(srcdir)/build

CPPFLAGS := -I$(builddir)/include
CFLAGS := -mlongcalls -mtext-section-literals
LDFLAGS := -mlsp=$(srcdir)/../xaf/hifi-dpf/build_hikey/hifi_hikey_lsp \
	-Wl,--defsym,_memmap_cacheattr_reset=0x24222222 \
	-Wl,--defsym,xrp_dsp_comm_base_magic=0x20161006

LIBS := $(builddir)/lib/libxrp-dsp.a  $(builddir)/lib/libxrp-dsp-hw-simple.a
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
	$(srcdir)/xrp/configure --prefix=$(builddir) --host=xtensa-elf CC=$(CC) --enable-dsp DSP_CORE=$(XTENSA_CORE) CFLAGS='-mlongcalls -mtext-section-literals' && \
	make && make install && touch $@
