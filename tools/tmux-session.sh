#!/bin/bash

# This script creates or attaches to a tmux session for the project.

# Get the absolute path of the project root directory
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

# The name for the tmux session
SESSION_NAME="yax86"

# Check if a tmux session with this name already exists
tmux has-session -t "$SESSION_NAME" 2>/dev/null

# If the session does not exist, create and configure it
if [ $? != 0 ]; then
  # Create a new detached session. Window 0 (shell) is created by default.
  # The -c flag sets the starting directory for the session.
  tmux new-session -d -s "$SESSION_NAME" -c "$PROJECT_ROOT"

  # Create window 1, name it "editor", and start Neovim
  tmux new-window -t "$SESSION_NAME":1 -n "editor" -c "$PROJECT_ROOT"
  tmux send-keys -t "$SESSION_NAME":editor "nvim" C-m

  # Create window 2, name it "gemini", and run the gemini command
  tmux new-window -t "$SESSION_NAME":2 -n "gemini" -c "$PROJECT_ROOT"
  tmux send-keys -t "$SESSION_NAME":gemini "gemini" C-m

  # Select window 0 to be the default active window
  tmux select-window -t "$SESSION_NAME":0
fi

# Attach to the (newly created or already existing) session
tmux attach-session -t "$SESSION_NAME"
