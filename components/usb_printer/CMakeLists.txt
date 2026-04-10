idf_component_register(
	SRCS "usb_printer.cpp" "usb_class_driver.cpp" "usb_daemon.cpp"
	INCLUDE_DIRS "."
    REQUIRES freertos log LinkedList usb)

