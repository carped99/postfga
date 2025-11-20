#!/bin/bash

# Buf Docker wrapper script
# This script provides a convenient way to run buf commands using Docker image
# when buf CLI is not installed locally.
#
# Usage:
#   ./scripts/buf-docker.sh generate        # Generate code
#   ./scripts/buf-docker.sh lint proto/     # Lint proto files
#   ./scripts/buf-docker.sh format -w .     # Format proto files
#
# Environment:
#   BUF_IMAGE - Docker image to use (default: bufbuild/buf:latest)
#   BUF_TAG   - Image tag (default: latest)
#   USE_LOCAL - Use local buf if available (default: true)

set -e

# Configuration
BUF_IMAGE="${BUF_IMAGE:-bufbuild/buf}"
BUF_TAG="${BUF_TAG:-latest}"
BUF_FULL_IMAGE="${BUF_IMAGE}:${BUF_TAG}"
USE_LOCAL="${USE_LOCAL:-true}"

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check for local buf installation
check_local_buf() {
    if [[ "$USE_LOCAL" == "true" ]] && command -v buf &> /dev/null; then
        log_info "Found local buf: $(buf --version)"
        return 0
    fi
    return 1
}

# Check for Docker installation
check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed. Please install Docker to use this script."
        log_error "Or install buf CLI: https://buf.build/docs/installation"
        exit 1
    fi
    log_info "Docker found: $(docker --version)"
}

# Pull latest buf image
pull_image() {
    log_info "Pulling buf image: $BUF_FULL_IMAGE"
    docker pull "$BUF_FULL_IMAGE" || {
        log_error "Failed to pull buf image"
        exit 1
    }
}

# Run buf with Docker
run_buf_docker() {
    local cmd=("$@")

    log_info "Running buf command with Docker image: $BUF_FULL_IMAGE"
    log_info "Working directory: $PROJECT_DIR"

    docker run --rm \
        --volume "$PROJECT_DIR:/workspace" \
        --workdir /workspace \
        --user "$(id -u):$(id -g)" \
        "$BUF_FULL_IMAGE" \
        "${cmd[@]}"
}

# Run buf locally
run_buf_local() {
    local cmd=("$@")

    log_info "Running buf command locally"
    log_info "Working directory: $PROJECT_DIR"

    cd "$PROJECT_DIR"
    buf "${cmd[@]}"
}

# Parse arguments
parse_args() {
    if [[ $# -eq 0 ]]; then
        show_usage
        exit 0
    fi

    case "$1" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --version|-v)
            if check_local_buf; then
                buf --version
            else
                check_docker
                docker run --rm "$BUF_FULL_IMAGE" --version
            fi
            exit 0
            ;;
        --local)
            USE_LOCAL="true"
            shift
            ;;
        --docker)
            USE_LOCAL="false"
            shift
            ;;
        --image)
            shift
            BUF_IMAGE="$1"
            shift
            ;;
        --no-pull)
            NO_PULL="true"
            shift
            ;;
        *)
            # Command to pass to buf
            return
            ;;
    esac

    parse_args "$@"
}

# Show usage
show_usage() {
    cat << EOF
Buf Docker wrapper - Run buf commands with Docker or local installation

Usage:
    $(basename "$0") [OPTIONS] [BUF_COMMAND] [BUF_ARGS...]

Options:
    -h, --help          Show this help message
    -v, --version       Show buf version
    --local             Use local buf if available (default)
    --docker            Force using Docker
    --image IMAGE       Use custom Docker image (default: bufbuild/buf:latest)
    --no-pull           Don't pull Docker image before running

Examples:
    # Generate code (uses local buf or Docker)
    $(basename "$0") generate

    # Lint proto files
    $(basename "$0") lint proto/

    # Format proto files
    $(basename "$0") format -w .

    # Break change detection
    $(basename "$0") breaking --against 'https://github.com/example/repo.git#branch=main'

    # Force Docker image
    $(basename "$0") --docker generate

    # Use specific image version
    $(basename "$0") --image bufbuild/buf:v1.27.0 lint

Commands:
    Common buf commands:
        generate        Generate code from proto files
        lint            Lint proto files
        format          Format proto files
        breaking        Check for breaking changes
        mod update      Update module dependencies
        ls-files        List proto files

For more information:
    https://buf.build/docs/cli-overview

EOF
}

# Main
main() {
    parse_args "$@"
    local remaining_args=("${@}")

    log_info "Buf Docker wrapper - version 1.0"

    if check_local_buf; then
        run_buf_local "${remaining_args[@]}"
    else
        log_info "Local buf not found, using Docker image"
        check_docker

        if [[ "${NO_PULL:-false}" != "true" ]]; then
            pull_image
        fi

        run_buf_docker "${remaining_args[@]}"
    fi
}

# Run main with all arguments
main "$@"
