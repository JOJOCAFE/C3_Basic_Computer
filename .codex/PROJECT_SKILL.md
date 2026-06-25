# C3 BASIC COMPUTER Project Skill

This file is the persistent project skill record for the repository.

## Manifesto

Play.
Hack.
Build.
Share.
Stay Curious.
Keep Going.

## Design Principles

1. Computer First  
   C3 BASIC COMPUTER is a computer. The microcontroller is an implementation detail.

2. Instant On  
   Power on. READY. Start creating.

3. Programming First  
   Programming is the primary interface.

4. Playable  
   The system should invite experimentation. The first program should take minutes, not hours.

5. Hackable  
   Every practical layer should be open to exploration.

6. Recoverable  
   Exploration should never permanently damage the computer.  
   The system software is protected. The user workspace is renewable.

7. Transparent  
   The system should reveal how it works.

8. Companion  
   The computer communicates clearly. It encourages exploration and never gets in the maker's way.

9. Honor Computing Traditions  
   Preserve timeless ideas when they continue to help understanding.  
   READY. HELLO, WORLD! ERROR. HELP.

10. Build for Makers  
    Designed for people who build, modify, experiment, and share.

11. Reference, Not Restriction  
    This project provides a reference implementation. Others are encouraged to modify, extend, and experiment.

12. Keep It Small  
    Prefer simple systems over complicated ones.  
    A feature is complete when it is understandable.

## Use Rule

When implementing or planning, treat this file as the authoritative behavior contract:

- Keep changes aligned with the manifesto and design principles.
- Prefer incremental milestones.
- Preserve recoverability and simple user-facing behavior.
- Do not remove core shell/BASIC foundations without a replacement.

## Canonical Architectural Contract

### Source anchors

- `docs/02_Software_Architecture.md`
- `docs/03_User_Experience_Guide.md`

### 02 Software Architecture (core)

- Layered architecture with strict single responsibility:
  - Computer (runtime), Shell, Workspace, Native RISC-V ASM Engine, BASIC Language, BASIC Library, Maker Programs.
- Lower layers provide services; upper layers do not depend on implementation details.
- Reference platform is ESP32-C3 + ESP-IDF, with portability goals across C-series targets.
- Separation of concerns and stable interfaces are mandatory.
- Recoverability: system software remains protected, workspace is recoverable.
- Architectural baseline for planning:
  - Shell is default environment.
  - Workspace supports editing and running text-based programs.
  - Native assembly is first-class and inspectable.

### 03 User Experience Guide (behavior)

- READY-state behavior:
  - Boot into a calm, familiar prompt (`READY.`).
- UX tone:
  - Clear, brief, honest, non-judgemental.
  - Avoid noise, avoid exaggerated wording.
- Error/warning behavior:
  - Errors explain what/where; avoid numeric-only codes.
  - Warnings only for potentially destructive actions, with explicit confirmation.
- Consistency:
  - Keep command behavior simple and predictable.
  - Return to READY after successful operations when practical.
- First impression:
  - power-on should enable immediate creation and exploration quickly.
