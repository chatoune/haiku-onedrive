#!/bin/bash
# Auto-documentation update script for Haiku OneDrive
# This script should be run after any code changes to maintain up-to-date documentation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "🔧 Updating Haiku OneDrive Documentation..."
echo "Project root: $PROJECT_ROOT"

# Ensure we're in project root
cd "$PROJECT_ROOT"

# Check if Doxygen is available
if ! command -v doxygen &> /dev/null; then
    echo "❌ Error: Doxygen not found. Please install doxygen to generate documentation."
    exit 1
fi

# Generate API documentation
echo "📚 Generating API documentation..."
doxygen Doxyfile

# Check if documentation was generated successfully
if [ -f "documentation/api/html/index.html" ]; then
    echo "✅ API documentation generated successfully"
    echo "   📖 View at: documentation/api/html/index.html"
else
    echo "❌ Error: API documentation generation failed"
    exit 1
fi

# Optionally build the project to ensure everything is in sync
if [ -d "build" ]; then
    echo "🔨 Building project to ensure consistency..."
    cd build
    make -j$(nproc) onedrive_daemon
    echo "✅ Build completed"
    cd ..
fi

echo "🎉 Documentation update completed successfully!"
echo ""
echo "📋 Documentation files updated:"
echo "   - documentation/api/html/ (API documentation)"
echo "   - All Doxygen comments in source files"
echo ""
echo "💡 Tips:"
echo "   - Open documentation/api/html/index.html in a web browser"
echo "   - Use the search function to find specific classes/functions"
echo "   - Documentation is automatically updated with this script"