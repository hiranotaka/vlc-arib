# sparkle

SPARKLE_VERSION := 1.5b6
SPARKLE_URL := http://sparkle.andymatuschak.org/files/Sparkle%20$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/sparkle-$(SPARKLE_VERSION).zip:
	$(call download,$(SPARKLE_URL))

.sum-sparkle: sparkle-$(SPARKLE_VERSION).zip

sparkle: sparkle-$(SPARKLE_VERSION).zip .sum-sparkle
	$(RM) -R $@ && mkdir -p $@ && cd $@ && unzip ../$<
	cd $@/Extras/Source\ Code/Configurations && \
		sed -i.orig -e s/"GCC_TREAT_WARNINGS_AS_ERRORS = YES"/"GCC_TREAT_WARNINGS_AS_ERRORS = NO"/g \
                    -e s/"ARCHS = .*"/"ARCHS = $(ARCH)"/  ConfigCommonRelease.xcconfig && \
		sed -i.orig -e s/MacOSX10.5.sdk/MacOSX$(OSX_VERSION).sdk/g ConfigCommon.xcconfig
	touch $@

.sparkle: sparkle
	cd $</Extras/Source\ Code && $(MAKE) && xcodebuild $(XCODE_FLAGS)
	cd $</Extras/Source\ Code && install_name_tool -id @executable_path/../Frameworks/Sparkle.framework/Versions/A/Sparkle build/Release/Sparkle.framework/Sparkle
	cd $< && cp -R Extras/Source\ Code/build/Release/Sparkle.framework "$(PREFIX)"
	touch $@
