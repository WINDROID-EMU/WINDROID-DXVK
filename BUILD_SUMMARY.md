# Resumo do Sistema de Build do DXVK

## 🏗️ **Sistema de Build Atual**

### **Tecnologias Utilizadas**
- **Meson**: Sistema de build principal
- **Ninja**: Backend de build (mais rápido que Make)
- **Cross-compilation**: MinGW-w64 para builds Windows
- **Native builds**: GCC/Clang para builds Linux

### **Workflows do GitHub Actions**

#### 1. **artifacts.yml** (Build Principal)
```yaml
jobs:
  artifacts-mingw-w64:     # Build Windows (MinGW)
    runs-on: ubuntu-latest
    uses: Joshua-Ashton/arch-mingw-github-action@v8
    
  artifacts-steamrt-sniper: # Build Linux (Steam Runtime)
    runs-on: ubuntu-latest
    container: steamrt/sniper/sdk:beta
    
  merge-artifacts:          # Merge dos artifacts
    needs: [artifacts-mingw-w64, artifacts-steamrt-sniper]
```

#### 2. **test-build-windows.yml** (Teste Windows)
```yaml
jobs:
  build-set-windows:
    runs-on: windows-latest
    steps:
    - setup glslangValidator
    - setup Meson
    - find Visual Studio
    - build MSVC x86/x64
```

### **Scripts de Build**

#### **package-release.sh** (Windows)
```bash
# Uso: ./package-release.sh version destdir [opções]
./package-release.sh 2.7.0 build --no-package

# Opções:
--no-package     # Não criar tar.gz
--dev-build      # Build de desenvolvimento
--build-id       # Incluir build ID
--64-only        # Apenas 64-bit
--32-only        # Apenas 32-bit
```

#### **package-native.sh** (Linux)
```bash
# Uso: ./package-native.sh version destdir [opções]
./package-native.sh 2.7.0 build

# Mesmas opções do package-release.sh
```

### **Arquivos de Configuração**

#### **build-win32.txt** (32-bit)
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

#### **build-win64.txt** (64-bit)
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

## 🚀 **Como Gerar Builds**

### **1. Build Manual**

#### **Pré-requisitos**
```bash
# Ubuntu/Debian
sudo apt install mingw-w64 meson ninja-build glslang-tools

# Arch Linux
sudo pacman -S mingw-w64-gcc meson ninja glslang
```

#### **Build Windows**
```bash
# Build completo (32-bit + 64-bit)
./package-release.sh 2.7.0 build

# Build apenas 64-bit
./package-release.sh 2.7.0 build --64-only

# Build de desenvolvimento
./package-release.sh 2.7.0 build --dev-build --no-package
```

#### **Build Linux**
```bash
# Build completo
./package-native.sh 2.7.0 build

# Build apenas 64-bit
./package-native.sh 2.7.0 build --64-only
```

### **2. Build com Meson Direto**

#### **Setup**
```bash
# Windows (64-bit)
meson setup --cross-file build-win64.txt --buildtype release build-win64

# Linux
meson setup --buildtype release build-linux
```

#### **Compilação**
```bash
# Compilar
ninja -C build-win64

# Instalar
ninja -C build-win64 install
```

### **3. Build com Otimizações de Adreno**

#### **Script Automático**
```bash
# Build completo com otimizações
./build-adreno.sh

# Build Windows 64-bit apenas
./build-adreno.sh -t windows -a 64

# Build com configuração customizada
./build-adreno.sh -c custom.conf -o
```

#### **Manual**
```bash
# Aplicar otimizações
./install-adreno-optimizations.sh

# Build com configuração
export DXVK_CONFIG_FILE=dxvk-adreno.conf
./package-release.sh 2.7.0 build
```

## 📁 **Estrutura de Saída**

### **Windows Build**
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

### **Linux Build**
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

## ⚙️ **Configurações de Build**

### **Flags de Compilação**

#### **Windows (MinGW)**
```bash
# Compiler flags
-msse -msse2 -msse3 -mfpmath=sse
-DNOMINMAX -D_WIN32_WINNT=0xa00

# Linker flags
-static -static-libgcc -static-libstdc++
-Wl,--file-alignment=4096
```

#### **Windows (MSVC)**
```bash
# Compiler flags
/Z7

# Linker flags
/FILEALIGN:4096 /DEBUG:FULL
```

#### **Linux**
```bash
# Compiler flags
-msse -msse2 -msse3 -mfpmath=sse

# Linker flags
-static-libgcc -static-libstdc++
```

### **Dependências**

#### **Windows**
- MinGW-w64 ou MSVC
- Vulkan Headers
- SPIRV Headers
- D3D8 SDK Headers (para D3D8)

#### **Linux**
- GCC/Clang
- Vulkan Headers
- SPIRV Headers
- SDL2/GLFW (para WSI)
- libdisplay-info

## 🔧 **Otimizações para Adreno 6xx**

### **Configurações Automáticas**
```bash
# Detecção automática de GPU Adreno 6xx
# Aplicação automática de otimizações:
- Limitação de VRAM (3GB)
- Cache de recursos dinâmicos
- Pipeline libraries habilitadas
- Barreiras relaxadas
- Tessellation limitada
- Latência otimizada
```

### **Build Otimizado**
```bash
# Script de build com otimizações
./build-adreno.sh -o

# Configuração manual
export DXVK_CONFIG_FILE=dxvk-adreno.conf
./package-release.sh 2.7.0 build
```

## 🐛 **Troubleshooting**

### **Problemas Comuns**

#### **Missing Vulkan-Headers**
```bash
sudo apt install vulkan-headers
```

#### **Missing SPIRV-Headers**
```bash
sudo apt install spirv-headers
```

#### **glslangValidator não encontrado**
```bash
sudo apt install glslang-tools
```

#### **MinGW-w64 não encontrado**
```bash
sudo apt install mingw-w64
```

### **Build Fails**

#### **Verificar dependências**
```bash
# Verificar se todas as dependências estão instaladas
meson setup --help

# Verificar cross-compilation
which x86_64-w64-mingw32-gcc
```

#### **Limpar build**
```bash
# Remover diretório de build
rm -rf build-win64

# Reconfigurar
meson setup --cross-file build-win64.txt build-win64
```

## 📊 **Performance de Build**

### **Tempos Estimados**
- **Windows 64-bit**: ~5-10 minutos
- **Windows 32-bit**: ~3-5 minutos
- **Linux**: ~3-8 minutos
- **Build completo**: ~15-25 minutos

### **Otimizações de Build**
- **Paralelização**: Ninja usa todos os cores disponíveis
- **Incremental**: Recompila apenas arquivos modificados
- **Cache**: Meson cache para builds mais rápidos

## 🔮 **Melhorias Futuras**

### **Sistema de Build**
- Suporte a builds ARM64
- Otimizações específicas por GPU
- Builds paralelos otimizados
- Cache de shaders integrado

### **Automação**
- Builds automáticos por commit
- Testes de regressão
- Deploy automático
- Notificações de build

### **Documentação**
- Guias específicos por plataforma
- Troubleshooting avançado
- Exemplos de configuração
- Benchmarks de build

## 📝 **Comandos Úteis**

### **Build Rápido**
```bash
# Build Windows 64-bit apenas
./package-release.sh 2.7.0 build --64-only

# Build Linux apenas
./package-native.sh 2.7.0 build --64-only

# Build com otimizações de Adreno
./build-adreno.sh -t windows -a 64 -o
```

### **Debug**
```bash
# Build de desenvolvimento
./package-release.sh 2.7.0 build --dev-build --no-package

# Build com debug symbols
meson setup --buildtype debug build-debug
```

### **Limpeza**
```bash
# Limpar builds
rm -rf build-*

# Limpar cache do Meson
rm -rf ~/.cache/meson
```

## 🎯 **Conclusão**

O sistema de build do DXVK é robusto e flexível, oferecendo:

1. **Multiplataforma**: Windows (cross-compilation) e Linux (native)
2. **Automação**: GitHub Actions para builds automáticos
3. **Flexibilidade**: Scripts para diferentes cenários
4. **Otimizações**: Suporte específico para GPUs Adreno 6xx
5. **Debugging**: Builds de desenvolvimento e debug symbols

As otimizações implementadas para Adreno 6xx são aplicadas automaticamente durante o build, garantindo melhor performance em dispositivos móveis sem comprometer a compatibilidade. 