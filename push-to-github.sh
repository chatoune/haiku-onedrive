#!/bin/bash
# Script to push Haiku OneDrive project to GitHub

echo "=== GitHub Repository Setup Instructions (SSH) ==="
echo ""
echo "1. Create a new repository on GitHub:"
echo "   - Go to https://github.com/new"
echo "   - Repository name: haiku-onedrive"
echo "   - Description: Native OneDrive client for Haiku OS with BFS attribute sync"
echo "   - Set as Public or Private based on your preference"
echo "   - DO NOT initialize with README, .gitignore, or license (we already have them)"
echo "   - Click 'Create repository'"
echo ""
echo "2. After creating the repository, add the SSH remote and push:"
echo ""
echo "   # Replace YOUR_USERNAME with your actual GitHub username"
echo "   git remote add origin git@github.com:YOUR_USERNAME/haiku-onedrive.git"
echo "   git push -u origin main"
echo ""
echo "Your SSH key is already configured in: /boot/home/config/settings/ssh/"
echo ""
echo "Current git status:"
git status
echo ""
echo "Current remotes:"
git remote -v
echo ""
echo "Ready to push these commits:"
git log --oneline -n 5