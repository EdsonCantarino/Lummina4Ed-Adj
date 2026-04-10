Troca automatica de logo.. criar logo e colar

.\compila.ps1 -Logo BIOSTERONS


Importante para ativar o JTAG nos dispositivos

https://esp32developer.com/programming-in-c-c/compilers-and-ides/esp-idf/visual-studio-code/4-debugging-with-vs-code
https://esp32developer.com/programming-in-c-c/compilers-and-ides/debuggers/efuse-programming
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/jtag-debugging/configure-other-jtag.html
https://esp32developer.com/programming-in-c-c/compilers-and-ides/debuggers/esp-prog-programmer-debugger

BUG FIX
https://github.com/espressif/esptool/issues/850


SEGUIR o VIDEO, importante porque tem configurações expecificas


Executar os comandos
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/jtag-debugging/index.html#jtag-debugging-run-openocd


COMANDO PRINCIPAL
openocd -f board/esp32s3-ftdi.cfg

Este comando desativa o DEBUG para o FreeRTOS
openocd -c 'set ESP_RTOS none' -f board/esp32s3-ftdi.cfg


Passo a passo para o IDE (Eclipse) - Deu ruim - Perdeu as configs

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/jtag-debugging/using-debugger.html
https://github.com/espressif/idf-eclipse-plugin/blob/master/docs/OpenOCD%20Debugging.md#esp-idf-gdb-openocd-debugging


