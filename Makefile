include common.makerules

SUPPORT_PACK=/home/disgruntledgm/src/avr/packs/atmega
IIC_LIB=/home/disgruntledgm/src/avr/iic_library

PNAME=project

OTHER_MODULES=build/iic.o build/iic_extras.o

MCU?=atmega328pb
MCUS?=atmega328p
CCOPTS?=
LNOPTS?=

CF=-Wall -g --std=c11 -Os -mmcu=$(MCU) -B $(SUPPORT_PACK)/gcc/dev/atmega328pb -I$(IIC_LIB)/include $(CCOPTS)
LF=-Wall -g --std=c11 -mmcu=$(MCU) -B $(SUPPORT_PACK)/gcc/dev/atmega328pb $(LNOPTS)
UF=-p atmega328pb -c atmelice_isp
OCF=-j .text -j .data -O ihex
ASF=--mcu=$(MCUS) --format=avr

#####################################################
# Silent mode by default                            #
# (set the environment variable VERBOSE to override #
#####################################################
ifndef VERBOSE
.SILENT:
endif

#####################
# Default Target    #
# Makes all modules #
# (no unit tests)   #
#####################
# Builds all modules and runs the final executable
$(PNAME).hex : $(PNAME).c common.h $(OTHER_MODULES)
	echo "$(T_COMP) $(PNAME).c -> $(PNAME).o"
	avr-gcc $(CF) -c $(PNAME).c $(CCOPTS)
	echo "$(T_LINK) $(PNAME).o -> $(PNAME).elf"
	avr-gcc $(LF) -o $(PNAME).elf $(PNAME).o $(OTHER_MODULES)
	echo "$(T_HEX) $(PNAME).elf -> $(PNAME).hex"
	avr-objcopy $(OCF) $(PNAME).elf $(PNAME).hex
	avr-size $(ASF) project.elf

build/iic.o : $(IIC_LIB)/src/iic.c $(IIC_LIB)/include/iic/iic.h build
	echo "$(T_COMP) iic.c -> iic.o"
	avr-gcc $(CF) -c $(IIC_LIB)/src/iic.c -o ./build/iic.o

build/iic_extras.o : $(IIC_LIB)/src/iic_extras.c $(IIC_LIB)/include/iic/iic_extras.h
	echo "$(T_COMP) iic_extras.c -> iic_extras.o"
	avr-gcc $(CF) -c $(IIC_LIB)/src/iic_extras.c -o build/iic_extras.o

UPLOAD : $(PNAME).hex
	echo "$(T_UPL) $(PNAME).hex"
	avrdude $(UF) -U flash:w:$(PNAME).hex

TERM :
	avrdude $(UF) -t

build: 
	mkdir build

################
# Util Targets #
################
# Cleans up compiled object files / binaries
clean :
	-rm $(PNAME).o $(PNAME).hex $(PNAME).elf
	-rm -r build
