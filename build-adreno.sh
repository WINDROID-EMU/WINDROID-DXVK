#!/bin/bash

# =============================================================================
# DXVK Adreno 6xx Build Script
# =============================================================================
# 
# Este script facilita o build do DXVK com otimizações específicas para
# GPUs Adreno 6xx, incluindo configurações de memória, pipeline e shader.
#
# =============================================================================

set -e

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Função para imprimir mensagens coloridas
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Função de ajuda
show_help() {
    echo "Uso: $0 [OPÇÕES]"
    echo ""
    echo "OPÇÕES:"
    echo "  -v, --version VERSION    Versão do DXVK (padrão: 2.7.0)"
    echo "  -d, --dest DIR           Diretório de destino (padrão: build)"
    echo "  -t, --type TYPE          Tipo de build: windows, linux, both (padrão: both)"
    echo "  -a, --arch ARCH          Arquitetura: 32, 64, both (padrão: both)"
    echo "  -c, --config FILE        Arquivo de configuração (padrão: dxvk-adreno.conf)"
    echo "  -o, --optimize           Aplicar otimizações específicas de Adreno"
    echo "  -n, --no-package         Não criar arquivo tar.gz"
    echo "  -D, --dev-build          Build de desenvolvimento"
    echo "  -h, --help               Mostrar esta ajuda"
    echo ""
    echo "EXEMPLOS:"
    echo "  $0                                    # Build completo com otimizações"
    echo "  $0 -t windows -a 64                   # Build Windows 64-bit apenas"
    echo "  $0 -t linux -c custom.conf            # Build Linux com config customizada"
    echo "  $0 -o -D                              # Build de desenvolvimento otimizado"
    echo ""
    echo "VARIÁVEIS DE AMBIENTE:"
    echo "  DXVK_VERSION            Versão do DXVK"
    echo "  DXVK_BUILD_DIR          Diretório de build"
    echo "  DXVK_CONFIG_FILE        Arquivo de configuração"
    echo "  CC, CXX                 Compiladores customizados"
}

# Variáveis padrão
DXVK_VERSION=${DXVK_VERSION:-"2.7.0"}
DXVK_BUILD_DIR=${DXVK_BUILD_DIR:-"build"}
BUILD_TYPE="both"
BUILD_ARCH="both"
CONFIG_FILE="dxvk-adreno.conf"
APPLY_OPTIMIZATIONS=false
NO_PACKAGE=false
DEV_BUILD=false

# Parse argumentos
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            DXVK_VERSION="$2"
            shift 2
            ;;
        -d|--dest)
            DXVK_BUILD_DIR="$2"
            shift 2
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -a|--arch)
            BUILD_ARCH="$2"
            shift 2
            ;;
        -c|--config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        -o|--optimize)
            APPLY_OPTIMIZATIONS=true
            shift
            ;;
        -n|--no-package)
            NO_PACKAGE=true
            shift
            ;;
        -D|--dev-build)
            DEV_BUILD=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Opção desconhecida: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validar argumentos
if [[ ! "$BUILD_TYPE" =~ ^(windows|linux|both)$ ]]; then
    print_error "Tipo de build inválido: $BUILD_TYPE"
    exit 1
fi

if [[ ! "$BUILD_ARCH" =~ ^(32|64|both)$ ]]; then
    print_error "Arquitetura inválida: $BUILD_ARCH"
    exit 1
fi

# Verificar se estamos no diretório correto
if [ ! -f "meson.build" ]; then
    print_error "Este script deve ser executado no diretório raiz do DXVK"
    exit 1
fi

print_status "Iniciando build do DXVK com otimizações de Adreno 6xx..."
print_status "Versão: $DXVK_VERSION"
print_status "Diretório: $DXVK_BUILD_DIR"
print_status "Tipo: $BUILD_TYPE"
print_status "Arquitetura: $BUILD_ARCH"

# Verificar dependências
print_status "Verificando dependências..."

# Verificar Meson
if ! command -v meson &> /dev/null; then
    print_error "Meson não encontrado. Instale com: sudo apt install meson"
    exit 1
fi

# Verificar Ninja
if ! command -v ninja &> /dev/null; then
    print_error "Ninja não encontrado. Instale com: sudo apt install ninja-build"
    exit 1
fi

# Verificar glslangValidator
if ! command -v glslangValidator &> /dev/null; then
    print_warning "glslangValidator não encontrado. Instale com: sudo apt install glslang-tools"
fi

# Verificar MinGW (para builds Windows)
if [[ "$BUILD_TYPE" == "windows" || "$BUILD_TYPE" == "both" ]]; then
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        print_error "MinGW-w64 não encontrado. Instale com: sudo apt install mingw-w64"
        exit 1
    fi
fi

print_success "Dependências verificadas"

# Aplicar otimizações se solicitado
if [ "$APPLY_OPTIMIZATIONS" = true ]; then
    print_status "Aplicando otimizações de Adreno 6xx..."
    
    # Verificar se o arquivo de configuração existe
    if [ ! -f "$CONFIG_FILE" ]; then
        print_warning "Arquivo de configuração $CONFIG_FILE não encontrado, criando..."
        cp dxvk-adreno.conf "$CONFIG_FILE" 2>/dev/null || {
            print_error "Não foi possível criar arquivo de configuração"
            exit 1
        }
    fi
    
    # Configurar variáveis de ambiente
    export DXVK_CONFIG_FILE="$CONFIG_FILE"
    print_success "Otimizações aplicadas via $CONFIG_FILE"
fi

# Preparar argumentos de build
BUILD_ARGS=""
if [ "$NO_PACKAGE" = true ]; then
    BUILD_ARGS="$BUILD_ARGS --no-package"
fi

if [ "$DEV_BUILD" = true ]; then
    BUILD_ARGS="$BUILD_ARGS --dev-build"
fi

if [ "$BUILD_ARCH" = "64" ]; then
    BUILD_ARGS="$BUILD_ARGS --64-only"
elif [ "$BUILD_ARCH" = "32" ]; then
    BUILD_ARGS="$BUILD_ARGS --32-only"
fi

# Criar diretório de build
mkdir -p "$DXVK_BUILD_DIR"

# Build Windows
if [[ "$BUILD_TYPE" == "windows" || "$BUILD_TYPE" == "both" ]]; then
    print_status "Iniciando build Windows..."
    
    if [ ! -f "package-release.sh" ]; then
        print_error "Script package-release.sh não encontrado"
        exit 1
    fi
    
    chmod +x package-release.sh
    
    print_status "Executando: ./package-release.sh $DXVK_VERSION $DXVK_BUILD_DIR $BUILD_ARGS"
    ./package-release.sh "$DXVK_VERSION" "$DXVK_BUILD_DIR" $BUILD_ARGS
    
    if [ $? -eq 0 ]; then
        print_success "Build Windows concluído"
    else
        print_error "Build Windows falhou"
        exit 1
    fi
fi

# Build Linux
if [[ "$BUILD_TYPE" == "linux" || "$BUILD_TYPE" == "both" ]]; then
    print_status "Iniciando build Linux..."
    
    if [ ! -f "package-native.sh" ]; then
        print_error "Script package-native.sh não encontrado"
        exit 1
    fi
    
    chmod +x package-native.sh
    
    print_status "Executando: ./package-native.sh $DXVK_VERSION $DXVK_BUILD_DIR $BUILD_ARGS"
    ./package-native.sh "$DXVK_VERSION" "$DXVK_BUILD_DIR" $BUILD_ARGS
    
    if [ $? -eq 0 ]; then
        print_success "Build Linux concluído"
    else
        print_error "Build Linux falhou"
        exit 1
    fi
fi

# Mostrar resultados
print_status "Build concluído! Resultados:"
echo ""

if [[ "$BUILD_TYPE" == "windows" || "$BUILD_TYPE" == "both" ]]; then
    if [ -d "$DXVK_BUILD_DIR/dxvk-$DXVK_VERSION" ]; then
        print_success "Windows build: $DXVK_BUILD_DIR/dxvk-$DXVK_VERSION"
        ls -la "$DXVK_BUILD_DIR/dxvk-$DXVK_VERSION"
    fi
fi

if [[ "$BUILD_TYPE" == "linux" || "$BUILD_TYPE" == "both" ]]; then
    if [ -f "$DXVK_BUILD_DIR/dxvk-native-$DXVK_VERSION.tar.gz" ]; then
        print_success "Linux build: $DXVK_BUILD_DIR/dxvk-native-$DXVK_VERSION.tar.gz"
        ls -la "$DXVK_BUILD_DIR/dxvk-native-$DXVK_VERSION.tar.gz"
    fi
fi

echo ""
print_success "Build do DXVK com otimizações de Adreno 6xx concluído com sucesso!"
print_status "Para usar:"
echo "  - Windows: Copie os arquivos .dll para o diretório do jogo"
echo "  - Linux: Extraia o arquivo .tar.gz e configure as variáveis de ambiente"
echo ""
print_status "Para testar as otimizações:"
echo "  export DXVK_CONFIG_FILE=$CONFIG_FILE"
echo "  wine seu_jogo.exe" 