release: flash_rescue_userspace
	strip flash_rescue_userspace

debug: flash_rescue_userspace
	clang-format --Werror -i --style=file *.c *.h
	clang-tidy -checks=bugprone\* *.c

flash_rescue_userspace: clean
	gcc *.c -o flash_rescue_userspace -Wall -Wextra -Werror -D_FORTIFY_SOURCE=2 -O2 -flto -mtune=native -march=native -fanalyzer -pie -fPIE -fstack-protector-strong -lz  -mshstk -fcf-protection=full

clean:
	rm -f flash_rescue_userspace
