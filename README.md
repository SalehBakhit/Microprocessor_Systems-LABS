# ECSE 426 - Microprocessor Systems Fall 2017
This course teaches necessary skills for building embedded processor-based systems, including the completion of a large-scale engineering project. The background knowledge taught in ECSE426 covers the basics of modern processor and system architectures, advanced use of tools such as assemblers, C compilers and debuggers in embedded systems, as well as the methods for peripherals interfacing and networking.
## COURSE AIM
	1. understand the organization and design principles behind modern microprocessor-based systems.
	2. be proficient in assembly and high-level (C language) programming for embedded systems.
	3. understand the performance impact of the embedded software, including the energy and memory-limited design techniques.
	4. know how to connect peripheral devices and networking interfaces, and how to write programs for the efficient interface use.
	5. have experience in developing a realistic embedded system solution through teamwork.
## LABS
STM32F4 Discovery Board:
- 32-bit ARM® Cortex® -M4 with FPU 
core, 1-Mbyte Flash memory, 192-
Kbyte RAM
- ST MEMS 3-axis accelerometer
- Omni-directional digital microphone

### LAB 1
Design a simple IIR Filter. Implement using both ARM’s Assembly and C, and compare efficiency.
    
    #### COMPONENTS USED:
    - ARM Cortex M4 Microprocessor
    - Keil uVision (C code)
### LAB 2
The purpose of this lab is to design and implement a voltmeter. The voltmeter measures AC/DC voltages and displays the rms values.
    
    #### COMPONENTS USED:
    	- ARM Cortex M4 Microprocessor
    	- 4-digit 7-segment displa
    	- Keil uVision (C code)
### LAB 3
The purpose of this lab is to control F4-discovery LEDs using MEMS accelerometer. The system is able to accept user input consisting of pitch and roll angles, and then display the measured tilt angles as well as control the brightness of 4 LEDs to represent the difference between user-entered angles and calculated angles.
   
   #### COMPONENTS USED:
    	- ARM Cortex M4 Microprocessor
        - MEMS accelerometer sensor LIS3DSH
        - 4-digit 7-segment display
        - 4x3 matrix keypad
        - Keil uVision (C code)
### LAB 4
Implement LAB 3 using RTOS. This is done to introduce OS controlled threads and components to better control the system.
