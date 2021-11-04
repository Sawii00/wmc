openocd -f ./STM32L4R9I-SensorTileBox.cfg -c "program build/wmc.elf verify reset" &
sleep 10
arm-none-eabi-gdb -ex 'file build/wmc.elf' -ex 'target extended-remote localhost:3333' -ex 'b main' -ex c -ex 'layout src'
