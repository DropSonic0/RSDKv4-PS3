# Guía para compilar RSDKv4 en PS3 (SDK 4.75)

Esta guía detalla los pasos necesarios para portar y compilar este motor en una consola PlayStation 3 utilizando el SDK oficial 4.75.

## Requisitos de Software

1.  **Sony PS3 SDK 4.75**: Ya lo tienes instalado.
2.  **Visual Studio 2013 o 2015**: El instalador del SDK 4.75 requiere una de estas versiones (2012, 2013 o 2015). Se recomienda **Visual Studio 2015 Professional** por su mejor soporte de estándares modernos de C++.
3. **Cygwin**: Requerido por el SDK de PS3 para las herramientas de compilación basadas en GCC/SN.
    - Al instalar, busca y selecciona:
        - `make` (en la categoría **Devel**).
        - `perl` (en la categoría **Perl** o **Interpreters**).
    - **Nota**: Solo selecciona los binarios, no necesitas los paquetes que dicen "-src" o "-debuginfo" a menos que sepas lo que haces.
4. **SN Systems / ProDG**: Estas herramientas suelen venir con el SDK y son las que permiten la depuración y comunicación con la consola (Target Manager).
5. **Python 2.7**: Muchos scripts de automatización del SDK oficial todavía dependen de Python 2.x.
6. **CMake**: Necesario para generar los archivos de proyecto de Visual Studio. Puedes descargarlo en [cmake.org](https://cmake.org/download/). **Asegúrate de marcar la opción "Add CMake to the system PATH" durante la instalación.**
7. **Windows 8.1 SDK**: Requerido por Visual Studio 2015 para que CMake pueda realizar las pruebas de compilación iniciales. Suele estar disponible como componente opcional en el instalador de Visual Studio.

## Solución de Problemas (Troubleshooting)

### Error MSB8036: The Windows SDK version 8.1 was not found
Este error ocurre porque Visual Studio 2015 necesita el SDK de Windows 8.1 para funcionar correctamente con CMake.
- **Solución**: Abre el "Panel de Control" -> "Programas y características", busca **Microsoft Visual Studio 2015**, haz clic en **Cambiar** (o Modificar) y asegúrate de marcar **Windows 8.1 SDK** en la lista de componentes a instalar.

### Error: No CMAKE_C_COMPILER could be found
Este error indica que CMake no puede encontrar el compilador de C++ de Visual Studio.
- **Solución**:
    1. Asegúrate de que instalaste el componente **Common Tools for Visual C++ 2015** al instalar Visual Studio. Si no, agrégalo desde el instalador.
    2. Asegúrate de ejecutar el comando de CMake desde un terminal limpio o, mejor aún, desde el **Developer Command Prompt for VS2015**.
    3. Si el error persiste, intenta instalar también el **Windows 10 SDK** (cualquier versión), ya que a veces CMake lo requiere para inicializar el entorno.

## Configuración del Proyecto

### Visual Studio
- **Versión**: Se recomienda **Visual Studio 2015 Professional**. La versión Express no soporta plugins de terceros como el de Sony (ProDG).
- **Integración**: Debes instalar el "Sony PlayStation(R)3 Integration" que viene con el SDK. Esto añadirá la plataforma "PS3" a Visual Studio.
- **Variables de Entorno**: Asegúrate de que `CELL_SDK` y `SN_PPU_TOOLCHAIN` estén correctamente configuradas en tu sistema.

## Inicio Rápido: ¿Qué ejecutar primero?

Una vez que tengas instalado el SDK 4.75 y Visual Studio 2015 Professional, sigue estos pasos para generar el proyecto:

1.  **Abrir una Terminal** (PowerShell o CMD).
2.  **Generar el Proyecto con CMake**: Ejecuta el siguiente comando en la carpeta raíz del motor (**NO** uses `-DCMAKE_TOOLCHAIN_FILE`, ya que esto puede causar errores):
    ```bash
    cmake -B build_ps3 -DPLATFORM=PS3 -G "Visual Studio 14 2015"
    ```
3.  **Abrir el Proyecto**:
    - Ve a la carpeta `build_ps3` que se acaba de crear.
    - Abre el archivo `RetroEngine.sln`.
4.  **Configurar la Plataforma en Visual Studio**:
    - En la barra superior, asegúrate de que la plataforma seleccionada sea **PS3** (esto es fundamental, no uses x64 o Win32).
    - Selecciona la configuración **Release** o **Debug**.
5.  **Compilar**: Haz clic derecho en el proyecto `RetroEngine` y selecciona **Build**.

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
