$(BUILD_DIR_ZIP)/chuni.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/chuni
	$(V)mkdir -p $(BUILD_DIR_ZIP)/chuni/DEVICE
	$(V)cp $(BUILD_DIR_32)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_32)/chunihook/chunihook.dll \
		$(DIST_DIR)/chuni/segatools.ini \
		$(DIST_DIR)/chuni/launch.bat \
		$(BUILD_DIR_ZIP)/chuni
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/chuni/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/chuni/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/chuni ; zip -r ../chuni.zip *

$(BUILD_DIR_ZIP)/cxb.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/cxb
	$(V)mkdir -p $(BUILD_DIR_ZIP)/cxb/DEVICE
	$(V)cp $(BUILD_DIR_32)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_32)/cxbhook/cxbhook.dll \
		$(DIST_DIR)/cxb/segatools.ini \
		$(DIST_DIR)/cxb/launch.bat \
		$(BUILD_DIR_ZIP)/cxb
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/cxb/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/cxb/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/cxb ; zip -r ../cxb.zip *
	
$(BUILD_DIR_ZIP)/diva.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/diva
	$(V)mkdir -p $(BUILD_DIR_ZIP)/diva/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/divahook/divahook.dll \
		$(DIST_DIR)/diva/segatools.ini \
		$(DIST_DIR)/diva/launch.bat \
		$(BUILD_DIR_ZIP)/diva
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/diva/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/diva/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/diva ; zip -r ../diva.zip *
	
$(BUILD_DIR_ZIP)/carol.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/carol
	$(V)mkdir -p $(BUILD_DIR_ZIP)/carol/DEVICE
	$(V)cp $(BUILD_DIR_32)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_32)/carolhook/carolhook.dll \
		$(DIST_DIR)/carol/segatools.ini \
		$(DIST_DIR)/carol/launch.bat \
		$(BUILD_DIR_ZIP)/carol
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/carol/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/carol/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/carol ; zip -r ../carol.zip *

$(BUILD_DIR_ZIP)/idz.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/idz
	$(V)mkdir -p $(BUILD_DIR_ZIP)/idz/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/idzhook/idzhook.dll \
		$(DIST_DIR)/idz/segatools.ini \
		$(DIST_DIR)/idz/launch.bat \
    	$(BUILD_DIR_ZIP)/idz
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/idz/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/idz/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/idz ; zip -r ../idz.zip *

$(BUILD_DIR_ZIP)/fgo.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/fgo
	$(V)mkdir -p $(BUILD_DIR_ZIP)/fgo/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/fgohook/fgohook.dll \
		$(DIST_DIR)/fgo/segatools.ini \
		$(DIST_DIR)/fgo/launch.bat \
    	$(BUILD_DIR_ZIP)/fgo
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/fgo/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/fgo/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/fgo ; zip -r ../fgo.zip *

$(BUILD_DIR_ZIP)/idac.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/idac
	$(V)mkdir -p $(BUILD_DIR_ZIP)/idac/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/idachook/idachook.dll \
		$(DIST_DIR)/idac/segatools.ini \
		$(DIST_DIR)/idac/config_hook.json \
		$(DIST_DIR)/idac/launch.bat \
    	$(BUILD_DIR_ZIP)/idac
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/idac/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/idac/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/idac ; zip -r ../idac.zip *

$(BUILD_DIR_ZIP)/swdc.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/swdc
	$(V)mkdir -p $(BUILD_DIR_ZIP)/swdc/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/swdchook/swdchook.dll \
		$(DIST_DIR)/swdc/segatools.ini \
		$(DIST_DIR)/swdc/config_hook.json \
		$(DIST_DIR)/swdc/launch.bat \
    	$(BUILD_DIR_ZIP)/swdc
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/swdc/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/swdc/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/swdc ; zip -r ../swdc.zip *

$(BUILD_DIR_ZIP)/mercury.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/mercury
	$(V)mkdir -p $(BUILD_DIR_ZIP)/mercury/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/mercuryhook/mercuryhook.dll \
		$(DIST_DIR)/mercury/segatools.ini \
		$(DIST_DIR)/mercury/launch.bat \
    	$(BUILD_DIR_ZIP)/mercury
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/mercury/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/mercury/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/mercury ; zip -r ../mercury.zip *

$(BUILD_DIR_ZIP)/chusan.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/chusan
	$(V)mkdir -p $(BUILD_DIR_ZIP)/chusan/DEVICE
	$(V)cp $(DIST_DIR)/chusan/segatools.ini \
		$(DIST_DIR)/chusan/config_hook.json \
		$(DIST_DIR)/chusan/launch.bat \
		$(BUILD_DIR_ZIP)/chusan
	$(V)cp $(BUILD_DIR_32)/chusanhook/chusanhook.dll \
		$(BUILD_DIR_ZIP)/chusan/chusanhook_x86.dll
	$(V)cp $(BUILD_DIR_64)/chusanhook/chusanhook.dll \
		$(BUILD_DIR_ZIP)/chusan/chusanhook_x64.dll
	$(V)cp $(BUILD_DIR_32)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_ZIP)/chusan/inject_x86.exe
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_ZIP)/chusan/inject_x64.exe
	$(V)cp pki/billing.pub \
		pki/ca.crt \
		$(BUILD_DIR_ZIP)/chusan/DEVICE
	for x in exe dll; do strip $(BUILD_DIR_ZIP)/chusan/*.$$x; done
	$(V)cd $(BUILD_DIR_ZIP)/chusan ; zip -r ../chusan.zip *

$(BUILD_DIR_ZIP)/mu3.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/mu3
	$(V)mkdir -p $(BUILD_DIR_ZIP)/mu3/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/mu3hook/mu3hook.dll \
		$(DIST_DIR)/mu3/segatools.ini \
		$(DIST_DIR)/mu3/launch.bat \
    	$(BUILD_DIR_ZIP)/mu3
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/mu3/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/mu3/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/mu3 ; zip -r ../mu3.zip *

$(BUILD_DIR_ZIP)/mai2.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/mai2
	$(V)mkdir -p $(BUILD_DIR_ZIP)/mai2/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/mai2hook/mai2hook.dll \
		$(DIST_DIR)/mai2/segatools.ini \
		$(DIST_DIR)/mai2/launch.bat \
    	$(BUILD_DIR_ZIP)/mai2
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/mai2/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/mai2/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/mai2 ; zip -r ../mai2.zip *

$(BUILD_DIR_ZIP)/cm.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/cm
	$(V)mkdir -p $(BUILD_DIR_ZIP)/cm/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/cmhook/cmhook.dll \
		$(DIST_DIR)/cm/config_hook.json \
		$(DIST_DIR)/cm/segatools.ini \
		$(DIST_DIR)/cm/launch.bat \
    	$(BUILD_DIR_ZIP)/cm
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/cm/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/cm/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/cm ; zip -r ../cm.zip *

$(BUILD_DIR_ZIP)/tokyo.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/tokyo
	$(V)mkdir -p $(BUILD_DIR_ZIP)/tokyo/DEVICE
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_64)/tokyohook/tokyohook.dll \
		$(DIST_DIR)/tokyo/config_hook.json \
		$(DIST_DIR)/tokyo/segatools.ini \
		$(DIST_DIR)/tokyo/launch.bat \
    	$(BUILD_DIR_ZIP)/tokyo
	$(V)cp pki/billing.pub \
		pki/ca.crt \
    	$(BUILD_DIR_ZIP)/tokyo/DEVICE
	$(V)strip $(BUILD_DIR_ZIP)/tokyo/*.{exe,dll}
	$(V)cd $(BUILD_DIR_ZIP)/tokyo ; zip -r ../tokyo.zip *

$(BUILD_DIR_ZIP)/kemono.zip:
	$(V)echo ... $@
	$(V)mkdir -p $(BUILD_DIR_ZIP)/kemono
	$(V)mkdir -p $(BUILD_DIR_ZIP)/kemono/DEVICE
	$(V)cp $(DIST_DIR)/kemono/segatools.ini \
		$(DIST_DIR)/kemono/launch.bat \
		$(BUILD_DIR_ZIP)/kemono
	$(V)cp $(BUILD_DIR_32)/kemonohook/kemonohook.dll \
		$(BUILD_DIR_ZIP)/kemono/kemonohook_x86.dll
	$(V)cp $(BUILD_DIR_64)/kemonohook/kemonohook.dll \
		$(BUILD_DIR_ZIP)/kemono/kemonohook_x64.dll
	$(V)cp $(BUILD_DIR_32)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_ZIP)/kemono/inject_x86.exe
	$(V)cp $(BUILD_DIR_64)/subprojects/capnhook/inject/inject.exe \
		$(BUILD_DIR_ZIP)/kemono/inject_x64.exe
	$(V)cp pki/billing.pub \
		pki/ca.crt \
		$(BUILD_DIR_ZIP)/kemono/DEVICE
	for x in exe dll; do strip $(BUILD_DIR_ZIP)/kemono/*.$$x; done
	$(V)cd $(BUILD_DIR_ZIP)/kemono ; zip -r ../kemono.zip *

$(BUILD_DIR_ZIP)/doc.zip: \
		$(DOC_DIR)/config \
		$(DOC_DIR)/chunihook.md \
		$(DOC_DIR)/idzhook.md \
		| $(zipdir)/
	$(V)echo ... $@
	$(V)zip -r $@ $^

$(BUILD_DIR_ZIP)/segatools.zip: \
		$(BUILD_DIR_ZIP)/chuni.zip \
		$(BUILD_DIR_ZIP)/cxb.zip \
		$(BUILD_DIR_ZIP)/carol.zip \
		$(BUILD_DIR_ZIP)/diva.zip \
		$(BUILD_DIR_ZIP)/doc.zip \
		$(BUILD_DIR_ZIP)/idz.zip \
		$(BUILD_DIR_ZIP)/idac.zip \
		$(BUILD_DIR_ZIP)/swdc.zip \
		$(BUILD_DIR_ZIP)/mercury.zip \
		$(BUILD_DIR_ZIP)/chusan.zip \
		$(BUILD_DIR_ZIP)/mu3.zip \
		$(BUILD_DIR_ZIP)/mai2.zip \
		$(BUILD_DIR_ZIP)/cm.zip \
		$(BUILD_DIR_ZIP)/tokyo.zip \
		$(BUILD_DIR_ZIP)/fgo.zip \
		$(BUILD_DIR_ZIP)/kemono.zip \
		CHANGELOG.md \
		README.md \

	$(V)echo ... $@
	$(V)zip -j $@ $^
