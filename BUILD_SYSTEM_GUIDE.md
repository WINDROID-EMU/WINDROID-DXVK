# Guia do Sistema de Build do DXVK

## Visão Geral

O DXVK utiliza o sistema de build **Meson** com **Ninja** como backend, oferecendo builds multiplataforma para Windows (cross-compilation) e Linux (native).

## Estrutura do Sistema de Build

### 1. **Arquivos de Configuração Principal**

#### `meson.build` (Raiz)
- Configuração principal do projeto
- Define compiladores, flags e dependências
- Configura builds para Windows e Linux
- Versão atual: **2.7.0**

#### `meson_options.txt`
```ini
enable_dxgi    = true   # Build DXGI
enable_d3d8    = true   # Build D3D8  
enable_d3d9    = true   # Build D3D9
enable_d3d10   = true   # Build D3D10
enable_d3d11   = true   # Build D3D11
build_id       = false  # Build ID para debug
native_glfw    = auto   # GLFW WSI para DXVK Native
native_sdl2    = auto   # SDL2 WSI para DXVK Native
native_sdl3    = auto   # SDL3 WSI para DXVK Native
```

### 2. **Scripts de Build**

#### `package-release.sh` (Windows)
```bash
# Uso: ./package-release.sh version destdir [opções]
./package-release.sh 2.7.0 build --no-package
```

**Opções:**
- `--no-package`: Não criar arquivo tar.gz
- `--dev-build`: Build de desenvolvimento (sem strip)
- `--build-id`: Incluir build ID
- `--64-only`: Apenas build 64-bit
- `--32-only`: Apenas build 32-bit

#### `package-native.sh` (Linux)
```bash
# Uso: ./package-native.sh version destdir [opções]
./package-native.sh 2.7.0 build
```

### 3. **Arquivos de Cross-Compilation**

#### `build-win32.txt` (32-bit)
```ini
[binaries]
c = 'i686-w64-mingw32-gcc'
cpp = 'i686-w64-mingw32-g++'
ar = 'i686-w64-mingw32-ar'
strip = 'i686-w64-mingw32-strip'
windres = 'i686-w64-mingw32-windres'

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'windows'
cpu_family = 'x86'
cpu = 'x86'
endian = 'little'
```

#### `build-win64.txt` (64-bit)
```ini
[binaries]
c = 'x86_64-w64-mingw32-gcc'
cpp = 'x86_64-w64-mingw32-g++'
ar = 'x86_64-w64-mingw32-ar'
strip = 'x86_64-w64-mingw32-strip'
windres = 'x86_64-w64-mingw32-windres'

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
```

## Workflows do GitHub Actions

### 1. **artifacts.yml** (Build Principal)
```yaml
jobs:
  artifacts-mingw-w64:     # Build Windows (MinGW)
    runs-on: ubuntu-latest
    steps:
    - checkout code
    - build release (package-release.sh)
    - upload artifacts
    
  artifacts-steamrt-sniper: # Build Linux (Steam Runtime)
    runs-on: ubuntu-latest
    container: steamrt/sniper/sdk:beta
    steps:
    - checkout code
    - build release (package-native.sh)
    - upload artifacts
    
  merge-artifacts:          # Merge dos artifacts
    needs: [artifacts-mingw-w64, artifacts-steamrt-sniper]
    steps:
    - merge artifacts
```

### 2. **test-build-windows.yml** (Teste Windows)
```yaml
jobs:
  build-set-windows:
    runs-on: windows-latest
    steps:
    - setup glslangValidator
    - setup Meson
    - find Visual Studio
    - download D3D8 SDK headers
    - build MSVC x86
    - build MSVC x64
    - prepare artifacts
    - upload artifacts
```

## Processo de Build Detalhado

### 1. **Build Windows (Cross-Compilation)**

#### Pré-requisitos
```bash
# Ubuntu/Debian
sudo apt install mingw-w64 meson ninja-build

# Arch Linux
sudo pacman -S mingw-w64-gcc meson ninja
```

#### Comando de Build
```bash
# Build completo (32-bit + 64-bit)
./package-release.sh 2.7.0 build

# Build apenas 64-bit
./package-release.sh 2.7.0 build --64-only

# Build de desenvolvimento
./package-release.sh 2.7.0 build --dev-build --no-package
```

#### Estrutura de Saída
```
build/
└── dxvk-2.7.0/
    ├── x32/
    │   ├── d3d9.dll
    │   ├── d3d11.dll
    │   └── dxgi.dll
    └── x64/
        ├── d3d9.dll
        ├── d3d11.dll
        └── dxgi.dll
```

### 2. **Build Linux (Native)**

#### Pré-requisitos
```bash
# Ubuntu/Debian
sudo apt install build-essential meson ninja-build
sudo apt install libsdl2-dev libglfw3-dev

# Arch Linux
sudo pacman -S base-devel meson ninja
sudo pacman -S sdl2 glfw-x11
```

#### Comando de Build
```bash
# Build completo
./package-native.sh 2.7.0 build

# Build apenas 64-bit
./package-native.sh 2.7.0 build --64-only
```

#### Estrutura de Saída
```
build/
└── dxvk-native-2.7.0.tar.gz
    └── usr/
        ├── lib/
        │   └── libdxvk_d3d9.so
        │   └── libdxvk_d3d11.so
        │   └── libdxvk_dxgi.so
        └── lib32/
            └── libdxvk_d3d9.so
            └── libdxvk_d3d11.so
            └── libdxvk_dxgi.so
```

### 3. **Build Manual com Meson**

#### Setup do Build
```bash
# Windows (32-bit)
meson setup --cross-file build-win32.txt --buildtype release build-win32

# Windows (64-bit)
meson setup --cross-file build-win64.txt --buildtype release build-win64

# Linux
meson setup --buildtype release build-linux
```

#### Compilação
```bash
# Compilar
ninja -C build-win64

# Instalar
ninja -C build-win64 install
```

## Configurações de Build

### 1. **Flags de Compilação**

#### Windows (MinGW)
```bash
# Compiler flags
-msse -msse2 -msse3 -mfpmath=sse
-DNOMINMAX -D_WIN32_WINNT=0xa00

# Linker flags
-static -static-libgcc -static-libstdc++
-Wl,--file-alignment=4096
```

#### Windows (MSVC)
```bash
# Compiler flags
/Z7

# Linker flags
/FILEALIGN:4096 /DEBUG:FULL
```

#### Linux
```bash
# Compiler flags
-msse -msse2 -msse3 -mfpmath=sse

# Linker flags
-static-libgcc -static-libstdc++
```

### 2. **Dependências**

#### Windows
- MinGW-w64 ou MSVC
- Vulkan Headers
- SPIRV Headers
- D3D8 SDK Headers (para D3D8)

#### Linux
- GCC/Clang
- Vulkan Headers
- SPIRV Headers
- SDL2/GLFW (para WSI)
- libdisplay-info

### 3. **Shaders**
```bash
# Compilação de shaders GLSL
glslangValidator --quiet --target-env vulkan1.3
```

## Variáveis de Ambiente

### Build
```bash
# Compiladores customizados
CC=gcc
CXX=g++

# Versão
DXVK_VERSION=2.7.0

# Configuração
DXVK_CONFIG_FILE=dxvk.conf
```

### Debug
```bash
# Log level
DXVK_LOG_LEVEL=info

# HUD
DXVK_HUD=devinfo,fps,memory

# Shader dump
DXVK_SHADER_DUMP_PATH=/tmp/shaders
```

## Troubleshooting

### 1. **Problemas Comuns**

#### Erro: Missing Vulkan-Headers
```bash
# Ubuntu/Debian
sudo apt install vulkan-headers

# Arch Linux
sudo pacman -S vulkan-headers
```

#### Erro: Missing SPIRV-Headers
```bash
# Ubuntu/Debian
sudo apt install spirv-headers

# Arch Linux
sudo pacman -S spirv-headers
```

#### Erro: glslangValidator não encontrado
```bash
# Ubuntu/Debian
sudo apt install glslang-tools

# Arch Linux
sudo pacman -S glslang
```

### 2. **Build Fails**

#### Verificar dependências
```bash
# Verificar se todas as dependências estão instaladas
meson setup --help

# Verificar cross-compilation
which x86_64-w64-mingw32-gcc
```

#### Limpar build
```bash
# Remover diretório de build
rm -rf build-win64

# Reconfigurar
meson setup --cross-file build-win64.txt build-win64
```

## Otimizações para Adreno 6xx

### 1. **Build Customizado**
```bash
# Build com otimizações específicas
DXVK_CONFIG_FILE=dxvk-adreno.conf ./package-release.sh 2.7.0 build
```

### 2. **Flags de Compilação Adicionais**
```bash
# Para GPUs ARM/Adreno
meson setup --cross-file build-win64.txt \
  -Dc_args="-march=native -mtune=native" \
  -Dcpp_args="-march=native -mtune=native" \
  build-win64-adreno
```

### 3. **Configuração de Build**
```bash
# Criar build otimizado
./install-adreno-optimizations.sh
export DXVK_CONFIG_FILE=dxvk.conf
./package-release.sh 2.7.0 build-adreno
```

## Próximos Passos

### 1. **Melhorias no Build System**
- Suporte a builds ARM64
- Otimizações específicas por GPU
- Builds paralelos otimizados
- Cache de shaders integrado

### 2. **Automação**
- Builds automáticos por commit
- Testes de regressão
- Deploy automático
- Notificações de build

### 3. **Documentação**
- Guias específicos por plataforma
- Troubleshooting avançado
- Exemplos de configuração
- Benchmarks de build 