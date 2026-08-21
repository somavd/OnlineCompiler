# Online Compiler

A web-based multi-language code compiler. Users write C++, Python, or JavaScript in the
browser and execute it inside sandboxed Docker containers. Includes a submission history
and an admin dashboard for managing coding questions.

This repository is split into two top-level folders:

```
OnlineCompiler/
├── app/     # Implementation — C++ backend, frontend, build & deployment files
└── docs/    # Documentation — project report, specs, screenshots
```

## Quick start

```bash
cd app
mkdir build && cd build
cmake ..
make -j$(nproc)          # macOS: make -j$(sysctl -n hw.ncpu)
./online_compiler
```

Then open <http://localhost:3000>.

- **Full build/run/API details:** see [`app/README.md`](app/README.md)
- **Design report & specs:** see [`docs/`](docs/)
