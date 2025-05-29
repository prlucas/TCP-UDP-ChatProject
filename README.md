# C++ Secure Text Chat (TCP + UDP) with Password-Protected Rooms

Este proyecto implementa un sistema de **chat por texto** usando TCP y UDP en C++17, con soporte para salas protegidas por contraseña y seguridad contra conexiones no autorizadas mediante un token secreto compartido (`SHARED_SECRET`).

---

## Características

- Chat en tiempo real por texto entre múltiples clientes.
- Uso de **TCP** para registro, autenticación y asignación de salas.
- Uso de **UDP** para la transmisión rápida de mensajes.
- Verificación por `SHARED_SECRET` en todos los mensajes (TCP + UDP).
- Salas con contraseña opcional:
  - Si la contraseña está vacía, la sala es pública.
  - Si se define una, solo ingresan quienes la conozcan.
- Múltiples salas simultáneas, cada una con su propio puerto UDP.
- Detección y expulsión automática de usuarios duplicados.
- Limpieza de clientes inactivos cada 6 segundos.

---

## Requisitos

- **Visual Studio 2022** (o compatible con C++17)
- **Boost** (solo se usa `boost::asio`)
  - Librerías necesarias:
    - `boost_system.lib`
    - `ws2_32.lib` (nativa de Windows)

---

## Cómo compilar

1. Abrí la solución `.sln` incluida en este repositorio.
2. Asegurate de tener Boost correctamente instalado y referenciado:
   - Headers (`boost/`) y librerías (`*.lib`) deben estar disponibles.
3. Compilá desde Visual Studio directamente (ya está configurado con C++17 y todas las dependencias necesarias).
