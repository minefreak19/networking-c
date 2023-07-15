CFLAGS:=-Wall -Wextra -Werror -pedantic -std=c11

.PHONY: all
all: http icmp ip

%: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -o $@ $<

