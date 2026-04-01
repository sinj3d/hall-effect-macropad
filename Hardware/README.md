# Custom Hall-Effect Macropad

![Project Status: WIP](https://img.shields.io/badge/Status-Work_in_Progress-orange)
![Hardware: V1 Complete](https://img.shields.io/badge/Hardware-V1_Complete-success)
![Firmware: In Development](https://img.shields.io/badge/Firmware-In_Development-blue)

## Overview
A custom-engineered macropad utilizing entirely 3D-printed Hall-effect switches. This project demonstrates end-to-end hardware design, from schematic capture and PCB layout to physical assembly and mechanism tuning. [cite_start]The hardware serves as a development platform for writing custom peripheral firmware in C++ and Rust[cite: 4].

<div align="center">
  <img src="[Link to image of your physical soldered board]" alt="Physical PCB Assembly" width="600"/>
</div>

## Current Status & Next Steps
**Hardware V1 is fully assembled and mechanically functional.** The custom PCB has been fabricated, populated, and soldered. The 3D-printed magnetic switches are currently undergoing physical fine-tuning to optimize the keystroke smoothness and actuation tolerances. 

**Immediate Next Steps:**
* [cite_start]**Firmware Architecture:** Drafting the low-level firmware in C++/Rust [cite: 4] to handle analog matrix scanning and switch debouncing.
* **Actuation Calibration:** Implementing dynamic actuation point calibration based on the analog Hall-effect sensor outputs.
* **Enclosure Design:** Finalizing the CAD for the external housing.

## Hardware Specifications
* **PCB Design:** Custom 2-layer board designed in KiCad.
* **Switches:** Custom 3D-printed housings utilizing Hall-effect sensors and permanent magnets for frictionless, adjustable actuation.
* **Manufacturing:** Routing, ground pours, and component placement optimized for hand-soldering and mechanical stability.

## Repository Structure

This repository contains the complete hardware definition files needed to replicate the board.

* `/Hardware`
    * `/Gerbers` - Ready-to-manufacture fabrication files (Copper, Mask, Silkscreen, Edge Cuts, Drill files).
    * `macropad_schematic.pdf` - High-resolution electrical schematic for quick review.
    * `/KiCad_Project` - Raw `.kicad_pro`, `.kicad_sch`, and `.kicad_pcb` source files.
* `/CAD`
    * .STL files for the 3D-printed switch mechanisms.

## Visuals & Renders

| Schematic Design | 3D PCB Render |
| :---: | :---: |
| <img src="[Link to schematic image]" alt="Schematic" width="400"/> | <img src="[Link to 3D KiCad Render]" alt="3D Render" width="400"/> |