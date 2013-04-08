#-------------------------------------------------
#
# Project created by QtCreator 2013-02-20T12:25:21
#
#-------------------------------------------------

QT       += core gui declarative

TARGET = nandsee
TEMPLATE = app


SOURCES += main.cpp\
        nandseewindow.cpp \
    nandview.cpp \
    eventstream.cpp \
    event.cpp \
    eventitemmodel.cpp \
    qhexedit.cpp \
    qhexedit_p.cpp \
    xbytearray.cpp \
    commands.cpp \
    hexwindow.cpp \
    tapboardprocessor.cpp \
    tapboardprocessorprivate.cpp \
    nand.cpp \
    joiner.cpp \
    grouper.cpp \
    sorter.cpp \
    byteswap.cpp \
    histogramview.cpp

HEADERS  += nandseewindow.h \
    nandview.h \
    eventstream.h \
    event.h \
    event-struct.h \
    eventitemmodel.h \
    qhexedit.h \
    qhexedit_p.h \
    xbytearray.h \
    commands.h \
    hexwindow.h \
    tapboardprocessor.h \
    state.h \
    packet-struct.h \
    tapboardprocessorprivate.h \
    byteswap.h \
    nand.h \
    histogramview.h

FORMS    += nandseewindow.ui \
    hexwindow.ui
