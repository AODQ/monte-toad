#!/usr/bin/sh

if [ ! -d "scripts" ]; then
  echo -e "\e[31mError:\e[0m"
  echo -e "  please run scripts from outside of script directory; it would be"
  echo -e "  preferrable to run in the install location."
  echo -e "\n ** you are attempting to run here **"
  echo -e "- install/"
  echo -e "    - bin/"
  echo -e "    * \e[92mscripts\e[0m/"
  echo -e "\n ** instead try here **"
  echo -e "* \e[92minstall\e[0m/"
  echo -e "    - bin/"
  echo -e "    - scripts/"
  echo -e "\nIn other words you probably want to do \`cd ..\`"
  exit 1
fi

if [ -d "test-models" ]; then
  echo -e "\e[31mError:\e[0m"
  echo -e "  it seems that you have already ran initialize-models; delete the"
  echo -e "  test-models directory if you would like to run it again"
  exit 1
fi

git clone https://github.com/aodq/test-models
pushd test-models
./extract.sh
popd
