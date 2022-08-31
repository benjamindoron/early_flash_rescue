release: clean
	gcc *.c -o flash_rescue_userspace -Wall -Wextra -Werror -D_FORTIFY_SOURCE=2 -O2 -mtune=native -march=native -pie -fPIE -flto -fstack-protector-strong -mshstk -fcf-protection=full -lz -Wno-unused-result
	strip flash_rescue_userspace

debug: clean
	clang-tidy -checks=bugprone\* *.c
	gcc *.c -o flash_rescue_userspace -Wall -Wextra -Werror -D_FORTIFY_SOURCE=2 -O2 -mtune=native -march=native -pie -fPIE -flto -fstack-protector-strong -mshstk -fcf-protection=full -lz

clean:
	rm -f flash_rescue_userspace
