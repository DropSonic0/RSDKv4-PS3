# Guía para compilar RSDKv4 en PS3 (SDK 4.75)

Esta guía detalla los pasos necesarios para portar y compilar este motor en una consola PlayStation 3 utilizando el SDK oficial 4.75.

## Requisitos de Software

1.  **Sony PS3 SDK 4.75**: Ya lo tienes instalado.
2.  **Visual Studio 2012, 2013 o 2015**: El instalador del SDK 4.75 requiere una de estas versiones. Se recomienda **Visual Studio 2012 Ultimate** o **Visual Studio 2015 Professional**.
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
Este error suele ocurrir en Visual Studio 2015.
- **Solución**: Abre el "Panel de Control" -> "Programas y características", busca tu instalación de Visual Studio, haz clic en **Cambiar** (o Modificar) y asegúrate de marcar **Windows 8.1 SDK** en la lista de componentes a instalar.

### Error: No CMAKE_C_COMPILER could be found
Este error indica que CMake no puede encontrar el compilador de C++ de Visual Studio. **Por defecto, algunas versiones de Visual Studio NO instalan el soporte para C++.**
- **Solución detallada**:
    1. Ve al **Panel de Control** -> **Programas y características**.
    2. Busca tu versión de Visual Studio, haz clic derecho y selecciona **Cambiar**.
    3. En el instalador, selecciona **Modificar**.
    4. Asegúrate de que **Visual C++** esté marcado para instalar.
    5. Haz clic en **Actualizar** y espera a que termine.
    6. **MUY IMPORTANTE**: Para ejecutar CMake, no uses el CMD normal. Busca en el menú inicio el "**Developer Command Prompt**" correspondiente a tu versión de Visual Studio y ejecútalo como administrador. Luego navega hasta la carpeta del motor y reintenta el comando.

### Verificación del Compilador
Para confirmar si Visual Studio tiene instalado C++, escribe lo siguiente en el **Developer Command Prompt**:
```cmd
cl
```
- **Si sale un mensaje de error** como "'cl' no se reconoce...": Significa que el componente de C++ **NO** se instaló correctamente. Vuelve al instalador (paso 4 de la sección anterior) y asegúrate de marcar absolutamente todo lo que esté bajo "Visual C++".
- **Si sale un mensaje con la versión del compilador** (ej: "Microsoft (R) C/C++ Optimizing Compiler..."): Entonces el problema es de CMake. Intenta borrar la carpeta `build_ps3` por completo antes de reintentar.

## Configuración del Proyecto

### Visual Studio
- **Versión**: Se recomienda **Visual Studio 2012 Ultimate** o **Visual Studio 2015 Professional**. La versión Express no soporta plugins de terceros como el de Sony (ProDG).
- **Integración**: Debes instalar el "Sony PlayStation(R)3 Integration" que viene con el SDK. Esto añadirá la plataforma "PS3" a Visual Studio.
- **Variables de Entorno**: Asegúrate de que `CELL_SDK` y `SN_PPU_TOOLCHAIN` estén correctamente configuradas en tu sistema.

## Inicio Rápido: ¿Qué ejecutar primero?

Una vez que tengas instalado el SDK 4.75 y Visual Studio (2012 o 2015), sigue estos pasos para generar el proyecto:

1.  **Abrir una Terminal** (PowerShell o CMD).
2.  **Generar el Proyecto con CMake**: Ejecuta el comando correspondiente a tu versión:
    - **Para Visual Studio 2012**:
      ```bash
      cmake -B build_ps3 -DPLATFORM=PS3 -G "Visual Studio 11 2012"
      ```
    - **Para Visual Studio 2015**:
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
4.  **Dependencies**: Necesitarás descargar el código fuente de `libogg` y `libvorbis` y colocarlos en la carpeta `dependencies/`.

### Cómo solucionar errores de "Cannot find source file" (libogg/libvorbis)
Si CMake te da errores diciendo que no encuentra archivos en `dependencies/libogg` o `dependencies/libvorbis`:
1. Descarga **libogg** de [xiph.org](https://xiph.org/downloads/). Extrae el contenido en una carpeta llamada `libogg` dentro de `dependencies/`.
2. Descarga **libvorbis** de [xiph.org](https://xiph.org/downloads/). Extrae el contenido en una carpeta llamada `libvorbis` dentro de `dependencies/`.
3. La estructura debe ser:
    - `dependencies/libogg/src/bitwise.c`
    - `dependencies/libvorbis/lib/analysis.c`

## Compilación con CMake
Si prefieres usar CMake, puedes intentar generar un proyecto de Visual Studio 10 usando:
```bash
cmake -B build_ps3 -DPLATFORM=PS3 -G "Visual Studio 10"
```
Nota: Es posible que necesites un archivo de "toolchain" específico para que CMake encuentre el compilador de PS3.
