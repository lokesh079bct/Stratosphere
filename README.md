# Stratosphere: A High-Flying Game Engine

Stratosphere is a modern, modular, and high-performance game engine designed for creating 2D and 3D applications. Built with a focus on ease of use and extensibility, it provides developers with the tools needed to rapidly prototype and ship visually stunning experiences.

---

## 🚀 Building the Project

Follow the steps below to clone the repository and prepare your build environment. Stratosphere uses **CMake** as its build system.

---

## 📦 Prerequisites

Before proceeding, ensure the following dependencies are installed:

- **CMake** – Used to generate platform-specific build files  
- **Vulkan SDK** – Required for graphics and compute rendering

---

## 🔧 Clone the Repository

```bash
git clone https://github.com/SujalMainali/Stratosphere.git
cd Stratosphere

## Generate Build Files
mkdir build
cd build

## Build With Cmake
cmake ..
cmake --build .