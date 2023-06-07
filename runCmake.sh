cd build

export ARM="~/gcc-arm-none-eabi-10.3-2021.10"
cmake -DPICO_SDK_PATH=$PICO_SDK_PATH \
-DCMAKE_C_COMPILER=$ARM/bin/arm-none-eabi-gcc \
-DCMAKE_CXX_COMPILER=$ARM/bin/arm-none-eabi-g++ \
-DCMAKE_PROGRAM_PATH=$ARM/lib/gcc/arm-none-eabi/10.3.1 \
-DCMAKE_ASM_FLAGS=-march=armv6-m ..

cd ..
