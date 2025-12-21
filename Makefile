CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lncurses

TARGET = filebrowser
OBJS = main.o filemanager.o code_view.o ui_helpers.o control_panel.o debugger.o debug_view.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

main.o: main.c filemanager.h code_view.h ui_helpers.h control_panel.h debug_view.h
	$(CC) $(CFLAGS) -c main.c

filemanager.o: filemanager.c filemanager.h ui_helpers.h
	$(CC) $(CFLAGS) -c filemanager.c

code_view.o: code_view.c code_view.h ui_helpers.h
	$(CC) $(CFLAGS) -c code_view.c

ui_helpers.o: ui_helpers.c ui_helpers.h
	$(CC) $(CFLAGS) -c ui_helpers.c

control_panel.o: control_panel.c control_panel.h ui_helpers.h
	$(CC) $(CFLAGS) -c control_panel.c

debugger.o: debugger.c debugger.h
	$(CC) $(CFLAGS) -c debugger.c

debug_view.o: debug_view.c debug_view.h debugger.h ui_helpers.h
	$(CC) $(CFLAGS) -c debug_view.c

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
