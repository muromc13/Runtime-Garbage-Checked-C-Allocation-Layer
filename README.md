# Runtime Garbage Checked C Allocation Layer 🛠️

![GitHub release](https://img.shields.io/github/release/muromc13/Runtime-Garbage-Checked-C-Allocation-Layer.svg)
![Issues](https://img.shields.io/github/issues/muromc13/Runtime-Garbage-Checked-C-Allocation-Layer.svg)
![License](https://img.shields.io/github/license/muromc13/Runtime-Garbage-Checked-C-Allocation-Layer.svg)

Welcome to the **Runtime Garbage Checked C Allocation Layer** repository! This project offers a powerful tool to enhance memory management in C programs. It uses the `LD_PRELOAD` method to interpose and wrap all `malloc`-family calls. By embedding canary-guarded headers, it effectively catches double frees and buffer overflows. Additionally, it provides leak reporting at exit while ensuring thread and fork safety.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Topics](#topics)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)
- [Releases](#releases)

## Features

- **Memory Management**: Efficiently manage memory allocations and deallocations.
- **Double-Free Detection**: Identify and prevent double-free errors.
- **Buffer Overflow Protection**: Guard against buffer overflows with canary values.
- **Leak Reporting**: Automatically report memory leaks upon program exit.
- **Thread and Fork Safety**: Ensure safe operation in multi-threaded and forked environments.

## Installation

To get started, clone the repository to your local machine:

```bash
git clone https://github.com/muromc13/Runtime-Garbage-Checked-C-Allocation-Layer.git
cd Runtime-Garbage-Checked-C-Allocation-Layer
```

Next, build the project:

```bash
make
```

You can now set the `LD_PRELOAD` environment variable to use the interposer:

```bash
export LD_PRELOAD=./libmalloc_wrapper.so
```

## Usage

To use the Runtime Garbage Checked C Allocation Layer, simply run your C program with the `LD_PRELOAD` variable set. For example:

```bash
LD_PRELOAD=./libmalloc_wrapper.so ./your_program
```

This setup will ensure that all memory allocations in `your_program` will be monitored for errors.

## Topics

This project covers several important topics in memory management:

- **Buffer Overflow Protection**: Safeguards against unauthorized access to memory.
- **C**: The programming language this tool is built for.
- **Double-Free Detection**: Helps catch errors that can lead to crashes.
- **Fork-Safe**: Ensures safe operation in forked processes.
- **Heap Sanitizer**: Assists in identifying heap-related issues.
- **LD_PRELOAD**: A powerful method to interpose library calls.
- **Malloc Wrapper**: A wrapper around standard memory allocation functions.
- **Memory Leak**: Detects memory that is allocated but never freed.
- **Memory Management**: The overall practice of handling memory allocation and deallocation.

## Contributing

We welcome contributions to improve this project. To contribute, please follow these steps:

1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Make your changes and commit them.
4. Push your branch to your forked repository.
5. Create a pull request detailing your changes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contact

For any questions or feedback, feel free to reach out:

- **Author**: [Your Name](https://github.com/yourusername)
- **Email**: your.email@example.com

## Releases

You can find the latest releases of this project [here](https://github.com/muromc13/Runtime-Garbage-Checked-C-Allocation-Layer/releases). Please download and execute the files as needed.

---

We hope you find the **Runtime Garbage Checked C Allocation Layer** useful for your memory management needs. Your feedback is valuable to us as we strive to improve this tool further!