#!/bin/bash

MK_GENERATED_IMGS_PATH=generated

APP_VERSION=$(cat package.yaml | grep -w CONFIG_APP_VERSION | sed 's/[[:space:]\"]//g' | awk -F":" '{print $2}')


if  [ "${APP_VERSION}" == '' ];then
APP_VERSION=v1.0.0
fi


if [ "${CDK_VERSION}" != '' ];then
ELF_NAME=`ls Obj/*.elf`
OBJCOPY=csky-elfabiv2-objcopy
READELF=csky-elfabiv2-readelf
${OBJCOPY} -O binary ${ELF_NAME} ${MK_GENERATED_IMGS_PATH}/data/prim
${OBJCOPY} -O binary ${ELF_NAME} ${MK_GENERATED_IMGS_PATH}/data_tmp/prim
PRODUCT_BIN=cdk/script/product.exe
else
cp ${MK_GENERATED_IMGS_PATH}/data/prim ${MK_GENERATED_IMGS_PATH}/data_tmp
PRODUCT_BIN=${MK_GENERATED_IMGS_PATH}/product
chmod +x ${PRODUCT_BIN}
READELF=csky-abiv2-elf-readelf
ELF_NAME=`ls *.elf`
fi

${READELF} -S ${ELF_NAME}

echo "[INFO] Generated output files ..."
${PRODUCT_BIN} version
${PRODUCT_BIN} image ${MK_GENERATED_IMGS_PATH}/images.zip -i ${MK_GENERATED_IMGS_PATH}/data -l -v "${APP_VERSION}" -p
${PRODUCT_BIN} image ${MK_GENERATED_IMGS_PATH}/images.zip -v "${APP_VERSION}" -spk ${MK_GENERATED_IMGS_PATH}/aes_128_ccm.key -dt SHA1 -st AES_128_CCM -iv 101112131415161718191a1b -aad 000102030405060708090a0b0c0d0e0f10111213 -tlen 16
${PRODUCT_BIN} image ${MK_GENERATED_IMGS_PATH}/images.zip -e ${MK_GENERATED_IMGS_PATH} -x -kp
${PRODUCT_BIN} image ${MK_GENERATED_IMGS_PATH}/images_tmp.zip -i ${MK_GENERATED_IMGS_PATH}/data_tmp -l -v "${APP_VERSION}" -p
${PRODUCT_BIN} image ${MK_GENERATED_IMGS_PATH}/images_tmp.zip -v "${APP_VERSION}" -spk ${MK_GENERATED_IMGS_PATH}/aes_128_ccm.key -dt SHA1 -st AES_128_CCM -iv 101112131415161718191a1b -aad 000102030405060708090a0b0c0d0e0f10111213 -tlen 16
${PRODUCT_BIN} diff -f ${MK_GENERATED_IMGS_PATH}/images_tmp.zip ${MK_GENERATED_IMGS_PATH}/images_tmp.zip -r -v "${APP_VERSION}" -spk ${MK_GENERATED_IMGS_PATH}/aes_128_ccm.key -dt SHA1 -st AES_128_CCM -iv 101112131415161718191a1b -aad 000102030405060708090a0b0c0d0e0f10111213 -tlen 16 -o ${MK_GENERATED_IMGS_PATH}/fota.bin

rm ${MK_GENERATED_IMGS_PATH}/data_tmp/ -fr
rm ${MK_GENERATED_IMGS_PATH}/images_tmp.zip

cp ${MK_GENERATED_IMGS_PATH}/total_image.hex ${MK_GENERATED_IMGS_PATH}/total_image.hexf