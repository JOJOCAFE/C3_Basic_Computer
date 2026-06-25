# ESP32-C3 Computer Project

## Design Overview v0.1

### Vision

สร้างคอมพิวเตอร์เพื่อการเรียนรู้ที่สมบูรณ์ในตัวเอง โดยใช้ ESP32-C3 เป็น CPU หลัก

เป้าหมายคือให้ผู้เรียนสามารถ:

* เขียน BASIC
* ควบคุม GPIO และอุปกรณ์ภายนอก
* ศึกษาระบบไฟล์
* ศึกษา RISC-V Assembly
* เขียนและรัน Assembly จริงบน ESP32-C3
* ใช้งานเป็น Terminal สำหรับ RV8-GR ได้

โครงการได้รับแรงบันดาลใจจาก:

* BBC Micro
* Commodore 64
* MSX
* Pocket Computer
* ESP32 Retro Computer

---

# Development Strategy

ใช้แนวทาง Incremental Development

เริ่มจากระบบที่ใช้งานได้จริงก่อน แล้วค่อยเพิ่มฮาร์ดแวร์

## Phase 1 : Core System (PC Terminal)

ฮาร์ดแวร์

* ESP32-C3 Super Mini
* USB Serial

ใช้งานผ่าน PC Terminal

เช่น:

* TeraTerm
* Putty
* minicom

Architecture

PC Terminal
↕ USB Serial
ESP32-C3

ประกอบด้วย

* Command Shell
* LittleFS
* BASIC Interpreter
* Native ASM Engine

---

# Core Features

## Command Shell

เมื่อบูต

ESP32-C3 COMPUTER
Version 0.1

READY.

>

คำสั่งพื้นฐาน

HELP
DIR
LOAD
SAVE
DELETE
RUN
NEW

---

## File System

ใช้ LittleFS

โครงสร้าง

/
├── BASIC/
├── ASM/
├── CONFIG/
├── TEMP/

คำสั่ง

DIR
LOAD
SAVE
DELETE

---

# BASIC Interpreter

## Language Core

LET
PRINT
INPUT
IF
THEN
ELSE
FOR
NEXT
GOTO
GOSUB
RETURN
END

ตัวอย่าง

10 FOR I=1 TO 10
20 PRINT I
30 NEXT I

---

# Mathematics

รองรับ

ABS
INT
ROUND
MIN
MAX

SQRT

SIN
COS
TAN

ASIN
ACOS
ATAN
ATAN2

LOG
EXP

RND

PI
E

DEG
RAD

ตัวอย่าง

PRINT SIN(30)

หมายเหตุ

- มุมสำหรับ `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `ATAN2` ใช้หน่วยองศา
- `RAD(x)` แปลงองศาเป็นเรเดียน
- `DEG(x)` แปลงเรเดียนเป็นองศา

---

# GPIO Control

ใช้ได้สองรูปแบบ

## Alias

DWRITE "LED",1

## GPIO Number

DWRITE 8,1

คำสั่ง

PINMODE
DWRITE
DREAD

ตัวอย่าง

PINMODE 8,OUT
DWRITE 8,1

---

# Native RISC-V Assembly

จุดเด่นหลักของระบบ

ใช้ Assembly จริงของ ESP32-C3

ไม่ใช่ Assembly จำลอง

## Inline ASM

ตัวอย่าง

10 ASM ADD2
20   add a0,a0,a1
30   ret
40 ENDASM

50 PRINT CALLASM("ADD2",10,20)

ผลลัพธ์

30

---

# ASM Engine

ทำงานดังนี้

RISC-V Assembly
↓
Mini Assembler
↓
Machine Code
↓
IRAM
↓
Native Execution
↓
Return to BASIC

---

# Initial ASM Support

คำสั่งที่รองรับใน v0.1

Arithmetic

add
sub
mul
div
rem

Logic

and
or
xor

Shift

sll
srl
sra

Immediate

addi
andi
ori
xori

Pseudo Instructions

li
mv
nop
ret

---

# Educational Goal

ผู้เรียนสามารถศึกษา

* Registers
* Calling Convention
* Function Call
* Arithmetic Logic
* Bit Manipulation
* Assembly Optimization

โดยใช้ ESP32-C3 จริง

---

# Future Features

## Monitor Program

คำสั่ง

REG
MEM
DISASM
ASM
BREAK
STEP

---

## Disassembler

แปลง Machine Code กลับเป็น Assembly

ตัวอย่าง

00000000  addi a0,a0,1
00000004  ret

---

## RISC-V Simulator

ใช้สำหรับ

* Step Execution
* Breakpoint
* Register Inspection
* Memory Inspection

---

# Future Hardware

## Display

ST7796 Capacitive Touch

รองรับ

CLS
PSET
LINE
RECT
CIRCLE
TEXT

---

## Keyboard

Bluetooth Keyboard

---

## Audio

PAM8302

คำสั่ง

SOUND
PLAY

---

## Motion Sensor

LIS3DH

ฟังก์ชัน

ACCELX()
ACCELY()
ACCELZ()

---

## RV8 Integration

ESP32-C3 สามารถทำหน้าที่เป็น

* Terminal
* Monitor
* Program Loader
* Debug Console

ให้กับ RV8-GR ผ่าน UART

---

# Version Roadmap

v0.1

* Shell
* LittleFS
* BASIC
* GPIO

v0.2

* Native ASM
* CALLASM

v0.3

* Monitor
* Disassembler

v0.4

* Graphics

v0.5

* Audio

v0.6

* Bluetooth Keyboard

v1.0

Standalone ESP32-C3 Computer

พร้อม

* Display
* Keyboard
* Audio
* File System
* BASIC
* Native RISC-V Assembly
* RV8 Terminal
