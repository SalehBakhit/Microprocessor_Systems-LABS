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
### LAB 2
The purpose of this lab is to create a voltmeter using, and then display rms voltages. This is done by writing C code and loading it onto 
the microcontroller. This C code mapped all the necessary inputs and outputs to the ports of the 
microcontroller, as well as convert the value received from the adc to an rms voltage. Our desired 
accuracy was below 5% and we got a maximum error of 1% which is very accurate. This was 
because we focused on precision and did not focus on memory management as it was not an issue of 
concern. The display was also very stable with no flickering. Over all, the voltmeter that we designed 
exceeded our expectations in accuracy. However there were some improvements that could have 
been made to improve factors other than accuracy. 
    #### COMPONENTS USED:
    	- STM32F4 Discovery Board
        - 7-segment display