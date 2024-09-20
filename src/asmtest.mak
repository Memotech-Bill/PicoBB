all: asmtest_gcc.txt asmtest_pbb.txt
	diff -i -w -y --suppress-common-lines asmtest_gcc.txt asmtest_pbb.txt | more

asmtest: bbasmb_arm_v8m.c
	gcc -DTEST -g -o asmtest bbasmb_arm_v8m.c

asmtest_gcc.txt: asmtest_v8m.s
#	arm-none-eabi-gcc  -mcpu=cortex-m33 -mthumb -march=armv8-m.main+fp+dsp -mfloat-abi=softfp -mcmse -O1 -g -Wa,-al -o asmtest.o -c asmtest_v8m.s >asmtest_gcc.txt 2>&1
	arm-none-eabi-as -mcpu=cortex-m33 -mthumb -march=armv8-m.main+fp+dsp -mfpu=fp-armv8 -al asmtest_v8m.s >asmtest_gcc.txt 2>&1
#	more gcc_asm.txt

asmtest_pbb.txt: asmtest asmtest_v8m.s
	./asmtest asmtest_v8m.s >asmtest_pbb.txt
#	more pbb_asm.txt
