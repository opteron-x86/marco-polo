# Third-party components

Marco Polo is GPL-3.0 with the additional permissions in EXCEPTIONS.md. Individual dependencies retain their licenses.

| Component | Source / pin | License |
|---|---|---|
| alandtse CommonLibSSE-NG 13.1.0 | https://github.com/alandtse/CommonLibSSE-NG/tree/12d61f3c39f7d4ba1b1ab543df2bca0e613dc747 | GPL-3.0 with exceptions; bundled notices. |
| MinHook | https://github.com/TsudaKageyu/minhook/tree/c3fcafdc10146beb5919319d0683e44e3c30d537 | BSD-style; bundled notice includes HDE. |
| SKSE Menu Framework API | https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API | LGPL-2.1; header and license bundled. |
| nlohmann/json | Bundled single header, version declared in header | MIT. |
| DirectXMath, DirectXTK, fmt, spdlog, rapidcsv | vcpkg baseline `ee12231b20c95013c6638d845d04c91559a1d1ff`, manifest `vcpkg.json` | Permissive licenses copied from the actual build dependencies. |

The source archive includes Marco Polo, bundled headers and the CommonLib/MinHook source used for the build. Dependency manifests and build scripts identify the remaining source packages. CommonLib's HDE/MIT notices are included. No game assets, SkyUI source/SWF, Missives scripts, Bethesda Papyrus compiler or proprietary SDK is redistributed. Note records refer to Skyrim's existing note model.
