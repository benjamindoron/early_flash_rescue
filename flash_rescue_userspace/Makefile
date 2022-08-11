release: flash_rescue_userspace
	strip flash_rescue_userspace

debug: clean
	clang-tidy -checks=bugprone\* flash_rescue_userspace.c
	gcc flash_rescue_userspace.c -o flash_rescue_userspace -Wall -D_FORTIFY_SOURCE=2 -O2 -mtune=native -march=native -pie -fPIE -fstack-protector-strong -mshstk -fcf-protection=full -lz

flash_rescue_userspace: clean
	gcc flash_rescue_userspace.c -o flash_rescue_userspace -Wall -Wno-unused-result -D_FORTIFY_SOURCE=2 -O2 -mtune=native -march=native -pie -fPIE -fstack-protector-strong -mshstk -fcf-protection=full -lz

clean:
	rm -f flash_rescue_userspace
