# Custom Applications for Nanotube

This directory contains custom applications for use with the Nanotube project, enabling the creation of custom pipelines.

Each application folder is structured as follows:

```
.
├── name_of_the_application
│   ├── application.c
│   └── compile.sh
```

### File Descriptions

- **`application.c`**: Contains the source code for the application.
- **`compile.sh`**: Contains the commands to compile the application. You can also specify the name of the bus that the application will use.

### Supported Buses

The following buses are currently supported:

- **`sb`**: Simple Bus
- **`shb`**: SoftHub Bus
- **`x3rx`**: X3RX Bus (X3522 SmartNICs)
- **`open_nic`**: OpenNIC Bus

### Compilation Instructions

To compile an application:

1. Copy the entire `Custom_applications` folder into the Nanotube project directory.
2. Open a terminal and navigate to the folder of the application you want to compile.
3. Run the following command:

   ```bash
   ./compile.sh
   ```

### Notes

- Ensure the **bus name** and **application name** are correctly specified in the `compile.sh` file.
- If the compilation is successful, a folder ending with `_hls` will be created inside the application folder. This folder contains the Vivado IPs required to create a custom pipeline.
