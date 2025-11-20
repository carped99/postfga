#!/bin/bash
# Build Docker images for PostFGA

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Default values
IMAGE_NAME="postfga/postgres"
VERSION="1.0.0"
PG_VERSION="18"
BUILD_TYPE="production"
PUSH_IMAGE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --dev|--development)
            BUILD_TYPE="development"
            shift
            ;;
        --prod|--production)
            BUILD_TYPE="production"
            shift
            ;;
        --push)
            PUSH_IMAGE=true
            shift
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --pg-version)
            PG_VERSION="$2"
            shift 2
            ;;
        --name)
            IMAGE_NAME="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --dev, --development    Build development image (default: production)"
            echo "  --prod, --production    Build production image"
            echo "  --push                  Push image to registry after build"
            echo "  --version VERSION       Set image version (default: 1.0.0)"
            echo "  --pg-version VERSION    Set PostgreSQL version (default: 18)"
            echo "  --name NAME             Set image name (default: postfga/postgres)"
            echo "  --help                  Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 --prod --version 1.0.1"
            echo "  $0 --dev"
            echo "  $0 --prod --push --version 1.0.0"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Set Dockerfile based on build type
if [ "$BUILD_TYPE" = "development" ]; then
    DOCKERFILE="docker/Dockerfile.build"
    TAG="${IMAGE_NAME}:${PG_VERSION}-dev"
else
    DOCKERFILE="docker/Dockerfile.production"
    TAG="${IMAGE_NAME}:${PG_VERSION}-${VERSION}"
    TAG_LATEST="${IMAGE_NAME}:latest"
fi

echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         PostFGA Docker Image Builder              ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}Configuration:${NC}"
echo -e "  Build Type:       ${YELLOW}${BUILD_TYPE}${NC}"
echo -e "  Dockerfile:       ${YELLOW}${DOCKERFILE}${NC}"
echo -e "  Image Tag:        ${YELLOW}${TAG}${NC}"
if [ "$BUILD_TYPE" = "production" ]; then
    echo -e "  Latest Tag:       ${YELLOW}${TAG_LATEST}${NC}"
fi
echo -e "  PostgreSQL:       ${YELLOW}${PG_VERSION}${NC}"
echo -e "  Push to Registry: ${YELLOW}${PUSH_IMAGE}${NC}"
echo ""

# Verify Dockerfile exists
if [ ! -f "$DOCKERFILE" ]; then
    echo -e "${RED}ERROR: Dockerfile not found: ${DOCKERFILE}${NC}"
    exit 1
fi

# Build image
echo -e "${GREEN}Building Docker image...${NC}"
echo -e "${YELLOW}Command: docker build -f ${DOCKERFILE} -t ${TAG} --build-arg PG_MAJOR=${PG_VERSION} .${NC}"
echo ""

docker build \
    -f "$DOCKERFILE" \
    -t "$TAG" \
    --build-arg PG_MAJOR="$PG_VERSION" \
    --build-arg GRPC_VERSION="v1.60.0" \
    --build-arg PROTOBUF_VERSION="25.1" \
    .

BUILD_RESULT=$?

if [ $BUILD_RESULT -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo -e "${GREEN}  Image: ${TAG}${NC}"

    # Tag as latest for production builds
    if [ "$BUILD_TYPE" = "production" ]; then
        echo -e "${YELLOW}Tagging as latest...${NC}"
        docker tag "$TAG" "$TAG_LATEST"
        echo -e "${GREEN}  Latest: ${TAG_LATEST}${NC}"
    fi

    # Show image details
    echo ""
    echo -e "${YELLOW}Image Details:${NC}"
    docker images "$IMAGE_NAME" | head -2

    # Test image
    echo ""
    echo -e "${YELLOW}Testing image...${NC}"
    docker run --rm "$TAG" pg_config --version
    docker run --rm "$TAG" ls -lh "$(docker run --rm "$TAG" pg_config --pkglibdir)/postfga.so" 2>/dev/null || \
        echo -e "${YELLOW}  Note: postfga.so check skipped (may not exist in dev image)${NC}"

    # Push to registry if requested
    if [ "$PUSH_IMAGE" = true ]; then
        echo ""
        echo -e "${YELLOW}Pushing image to registry...${NC}"
        docker push "$TAG"

        if [ "$BUILD_TYPE" = "production" ]; then
            docker push "$TAG_LATEST"
        fi

        echo -e "${GREEN}✓ Image pushed successfully${NC}"
    fi

    # Success summary
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║              Build Complete!                       ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${GREEN}Next steps:${NC}"
    if [ "$BUILD_TYPE" = "production" ]; then
        echo -e "  ${YELLOW}# Run container${NC}"
        echo -e "  docker run -d --name postfga-test -e POSTGRES_PASSWORD=test ${TAG}"
        echo ""
        echo -e "  ${YELLOW}# Or use docker-compose${NC}"
        echo -e "  cd docker && docker-compose up -d"
    else
        echo -e "  ${YELLOW}# Use for development${NC}"
        echo -e "  docker run -it --rm -v \$(pwd):/workspace ${TAG} bash"
    fi
    echo ""

else
    echo ""
    echo -e "${RED}✗ Build failed!${NC}"
    echo -e "${RED}  Check the error messages above${NC}"
    exit 1
fi
