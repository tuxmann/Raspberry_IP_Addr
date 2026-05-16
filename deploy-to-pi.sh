#!/bin/bash
# Deploy Raspberry_IP_Addr to a Pi. Interactive menu or: ./deploy-to-pi.sh [user] [host]
# Prerequisites: ssh-copy-id jason@raspberrypi.local

set -e
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Non-interactive: accept user and host from args
PI_USER="${1:-jason}"
PI_HOST="${2:-raspberrypi.local}"

do_deploy() {
  echo "Deploying to ${PI_USER}@${PI_HOST}..."
  rsync -avz --exclude '.git' --exclude '__pycache__' --exclude '*.pyc' \
      --exclude '.venv' \
      "$PROJECT_DIR/" "${PI_USER}@${PI_HOST}:~/Raspberry_IP_Addr/"
  echo "Deploy done."
}

do_build_flash() {
  echo "Building on Pi and flashing ATtiny88..."
  ssh "${PI_USER}@${PI_HOST}" 'cd ~/Raspberry_IP_Addr && \
    (command -v avr-gcc >/dev/null 2>&1 || { echo "Install on Pi: sudo apt install gcc-avr avr-libc"; exit 1; }) && \
    make && \
    sudo avrdude -c linuxspi -P /dev/spidev0.0:/dev/gpiochip0 -p t88 -B 10 -U flash:w:bin/show_ip.hex:i'
  echo "Build and flash done."
}

do_install_service() {
  echo "Installing systemd service for auto-start..."
  ssh "${PI_USER}@${PI_HOST}" "sudo sed -e 's/jason/${PI_USER}/g' -e 's|/home/jason|/home/${PI_USER}|g' ~/Raspberry_IP_Addr/hostip.service | sudo tee /etc/systemd/system/hostip.service > /dev/null && sudo systemctl daemon-reload && sudo systemctl enable hostip && sudo systemctl start hostip && echo 'Service enabled and started.'"
  echo "Service installed and started."
}

# If first arg is a flag, run non-interactive (backward compatible)
case "$1" in
  --install-service)
    shift
    PI_USER="${1:-jason}"
    PI_HOST="${2:-raspberrypi.local}"
    do_deploy
    do_install_service
    exit 0
    ;;
  --help|-h)
    echo "Usage: $0 [user] [host]     # interactive menu"
    echo "       $0 --install-service [user] [host]   # deploy + install service (no menu)"
    exit 0
    ;;
esac

# Interactive menu
while true; do
  echo ""
  echo "Target: ${PI_USER}@${PI_HOST}"
  echo ""
  echo "  1) Deploy only (sync files to Pi)"
  echo "  2) Deploy + build firmware on Pi and flash ATtiny88"
  echo "  3) Deploy + install/start hostip systemd service"
  echo "  4) Deploy + build+flash + install service"
  echo "  5) Change user/host"
  echo "  q) Quit"
  echo ""
  read -r -p "Choice [1-5, q]: " choice
  case "$choice" in
    1)
      do_deploy
      ;;
    2)
      do_deploy
      do_build_flash
      ;;
    3)
      do_deploy
      do_install_service
      ;;
    4)
      do_deploy
      do_build_flash
      do_install_service
      ;;
    5)
      read -r -p "User [${PI_USER}]: " u
      PI_USER="${u:-$PI_USER}"
      read -r -p "Host [${PI_HOST}]: " h
      PI_HOST="${h:-$PI_HOST}"
      echo "Target set to ${PI_USER}@${PI_HOST}"
      ;;
    q|Q)
      echo "Bye."
      exit 0
      ;;
    *)
      echo "Invalid choice."
      ;;
  esac
done
