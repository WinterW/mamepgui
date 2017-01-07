include(common_settings.pri)

greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent widgets
DESTDIR = bin
TARGET = mamepgui
QT += xml

#INCLUDE
INCLUDEPATH += quazip lzma include include/zlib include/SDL include/SDL/$${OSDIR}

#LIBS
TARGETDEPS += \
	./lib/$${OSDIR}/libquazip.a \
	./lib/$${OSDIR}/liblzma.a

LIBS += -L./lib/$${OSDIR}
LIBS += -lquazip -llzma -lz

build_sdl {
	macx: LIBS += -framework Cocoa -framework IOKit -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework OpenGL -framework ForceFeedback
	LIBS += -lSDL
	DEFINES += USE_SDL
}

HEADERS += \
	prototype.h \
	mainwindow.h \
	screenshot.h\
	dialogs.h \
	audit.h \
	gamelist.h \
	mameopt.h \
	utils.h \
	processmanager.h\
	ips.h \
	m1.h \

SOURCES += \
	prototype.cpp \
	mainwindow.cpp \
	screenshot.cpp\
	dialogs.cpp \
	audit.cpp \
	gamelist.cpp \
	mameopt.cpp \
	utils.cpp \
	processmanager.cpp\
	ips.cpp \
	m1.cpp \

FORMS += \
	mainwindow.ui \
	playoptions.ui \
	options.ui \
	csvcfg.ui \
	directories.ui \
	about.ui \
	cmd.ui \
	ips.ui \
	m1.ui

TRANSLATIONS = \
	lang/mamepgui_zh_CN.ts \
	lang/mamepgui_zh_TW.ts \
	lang/mamepgui_ja_JP.ts \
	lang/mamepgui_es_ES.ts \
	lang/mamepgui_fr_FR.ts \
	lang/mamepgui_hu_HU.ts \
	lang/mamepgui_ko_KR.ts \
	lang/mamepgui_pt_BR.ts \
	lang/mamepgui_ru_RU.ts \
	lang/mamepgui_it_IT.ts

RESOURCES = mamepgui.qrc 

win32 {
	RC_FILE = mamepgui.rc
	QMAKE_LFLAGS_RELEASE += -static-libgcc
}

macx {
	ICON = mamepgui.icns
}
