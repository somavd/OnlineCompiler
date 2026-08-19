# Project Documentation

This folder holds the project's specs and design material.

```
docs/
├── project_report.tex   # Full design report (LaTeX source)
├── screenshot.png       # UI screenshot
└── README.md            # This file
```

## Compiling the LaTeX Report

### Prerequisites
Install a LaTeX distribution:
- **macOS**: `brew install --cask mactex` (or `brew install basictex` for minimal install)
- **Ubuntu/Debian**: `sudo apt install texlive-full`
- **Windows**: Install [MiKTeX](https://miktex.org/)

### Compile to PDF
```bash
cd docs
pdflatex project_report.tex
pdflatex project_report.tex   # Run twice for table of contents
```

The output will be `project_report.pdf`.

### Online Alternative
If you don't want to install LaTeX locally, upload `project_report.tex` to [Overleaf](https://www.overleaf.com/) for instant compilation.

## Contents
The report covers:
1. Introduction & Objectives
2. Technology Stack
3. System Architecture (with diagrams)
4. Data Flow Diagrams (Level 0, 1, 2)
5. Database Schema (SQLite)
6. API Documentation
7. User Manual
8. Deployment Guide
9. Testing Strategy
10. Future Enhancements
