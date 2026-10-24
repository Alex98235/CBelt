# Clangd Setup

For the best editing experience with clangd, create a local `.clangd` file in the project root (it's already in `.gitignore` so it stays machine-local):

**Windows (MinGW):**

```yaml
CompileFlags:
  Add: [-std=c11, -DCBELT_IMPLEMENTATION, --target=x86_64-w64-windows-gnu]
```

**Linux:**

```yaml
CompileFlags:
  Add: [-std=c11, -DCBELT_IMPLEMENTATION]
```

The `-DCBELT_IMPLEMENTATION` flag prevents the implementation section from being greyed out. The `--target` flag is only needed on Windows to help clangd find MinGW headers (`windows.h`, `HANDLE`, `CRITICAL_SECTION`, etc.).
