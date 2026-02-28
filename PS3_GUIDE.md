# Guía para compilar RSDKv4 en PS3 (SDK 4.75)

Esta guía detalla los pasos necesarios para portar y compilar este motor en una consola PlayStation 3 utilizando el SDK oficial 4.75.

## Requisitos de Software

1.  **Sony PS3 SDK 4.75**: Ya lo tienes instalado.
2.  **Visual Studio 2013 o 2015**: El instalador del SDK 4.75 requiere una de estas versiones (2012, 2013 o 2015). Se recomienda **Visual Studio 2015 Professional** por su mejor soporte de estándares modernos de C++.
3. **Cygwin**: Requerido por el SDK de PS3 para las herramientas de compilación basadas en GCC/SN. Asegúrate de incluir los paquetes de `make` y `perl`.
4. **SN Systems / ProDG**: Estas herramientas suelen venir con el SDK y son las que permiten la depuración y comunicación con la consola (Target Manager).
5. **Python 2.7**: Muchos scripts de automatización del SDK oficial todavía dependen de Python 2.x.

## Configuración del Proyecto

### Visual Studio
- **Versión**: Se recomienda **Visual Studio 2015 Professional**. La versión Express no soporta plugins de terceros como el de Sony (ProDG).
- **Integración**: Debes instalar el "Sony PlayStation(R)3 Integration" que viene con el SDK. Esto añadirá la plataforma "PS3" a Visual Studio.
- **Variables de Entorno**: Asegúrate de que `CELL_SDK` y `SN_PPU_TOOLCHAIN` estén correctamente configuradas en tu sistema.

### Cambios en el Código (Ya aplicados en esta rama)
- Se ha actualizado `RSDKv4/RetroEngine.hpp` para detectar la plataforma PS3.
- Se ha incluido `ps3_compat.h` para manejar funciones faltantes como `snprintf`.
- Se han configurado las rutas base de archivos en `RetroEngine.cpp` para apuntar a `/dev_hdd0/game/RSDKV4000/USRDIR/`.

### Pendientes para el Desarrollador
1.  **Renderer**: El motor usa OpenGL. En PS3 deberás usar **PSGL** (incluido en el SDK). Se han añadido los includes necesarios, pero es posible que debas ajustar algunas llamadas de OpenGL que no sean compatibles con el perfil ES 1.1/2.0 de PSGL.
2.  **Audio**: Actualmente el motor usa SDL. PS3 usa `libaudio`. Deberás implementar un backend en `Audio.cpp` que use `cellAudio`.
3.  **Input**: Deberás mapear los mandos de PS3 usando `libio` (`cellPad`).
4.  **Dependencies**: Necesitarás compilar `libogg` y `libvorbis` para PS3.

## Compilación con CMake
Si prefieres usar CMake, puedes intentar generar un proyecto de Visual Studio 10 usando:
```bash
cmake -B build_ps3 -DPLATFORM=PS3 -G "Visual Studio 10"
```
Nota: Es posible que necesites un archivo de "toolchain" específico para que CMake encuentre el compilador de PS3.
