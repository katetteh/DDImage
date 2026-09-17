# DDImage
An Image processing library used for building computer vision projects.

flowchart TD

subgraph group_app_ui["Application and Presentation"]
  node_app_bootstrap["Application Bootstrap<br/>entry point"]
  node_view_preparation["View Preparation<br/>converter"]
  node_interactive_viewer["Interactive Viewer<br/>Qt window"]
end

subgraph group_image_data["Image Data"]
  node_matrix_model["Matrix Data Model<br/>pixel data"]
  node_rgb_model["RGB Image Model<br/>colour data"]
end

subgraph group_processing["Image Processing"]
  node_processing_api["Image Processing API<br/>public API"]
  node_image_io["Image I/O Boundary<br/>[ImageProcessor.cpp]"]
  node_resize_kernels["Resize Dispatcher and Kernels<br/>interpolation"]
  node_filter_roi["Filter and ROI Engine<br/>filters"]
end

subgraph group_external_io["External Runtime and Files"]
  node_desktop_user(("Desktop User<br/>actor"))
  node_qt_widgets{{"Qt 6 Widgets<br/>GUI runtime"}}
  node_local_files["Local Image Files"]
end

node_desktop_user -->|"launches"| node_app_bootstrap
node_app_bootstrap -->|"creates app"| node_qt_widgets
node_app_bootstrap -->|"loads and resizes"| node_processing_api
node_processing_api -->|"uses I/O"| node_image_io
node_local_files -->|"provides bytes"| node_image_io
node_image_io -->|"decodes grayscale"| node_matrix_model
node_image_io -->|"decodes RGB"| node_rgb_model
node_processing_api -->|"dispatches resize"| node_resize_kernels
node_resize_kernels -->|"reads and creates"| node_matrix_model
node_processing_api -->|"exposes filters"| node_filter_roi
node_filter_roi -->|"processes planes"| node_matrix_model
node_app_bootstrap -->|"passes images"| node_view_preparation
node_view_preparation -->|"reads pixels"| node_matrix_model
node_view_preparation -->|"reads channels"| node_rgb_model
node_view_preparation -->|"creates QImages"| node_qt_widgets
node_app_bootstrap -->|"supplies entries"| node_interactive_viewer
node_interactive_viewer -->|"builds UI"| node_qt_widgets
node_interactive_viewer -->|"presents images"| node_desktop_user
node_desktop_user -->|"controls viewer"| node_interactive_viewer
node_image_io -->|"writes PNG"| node_local_files

click node_app_bootstrap "https://github.com/katetteh/ddimage/blob/main/main.cpp"
click node_view_preparation "https://github.com/katetteh/ddimage/blob/main/main.cpp"
click node_interactive_viewer "https://github.com/katetteh/ddimage/blob/main/main.cpp"
click node_matrix_model "https://github.com/katetteh/ddimage/blob/main/matrix.h"
click node_rgb_model "https://github.com/katetteh/ddimage/blob/main/ImageProcessor.h"
click node_processing_api "https://github.com/katetteh/ddimage/blob/main/ImageProcessor.h"
click node_image_io "https://github.com/katetteh/ddimage/blob/main/ImageProcessor.cpp"
click node_resize_kernels "https://github.com/katetteh/ddimage/blob/main/ImageProcessor.cpp"
click node_filter_roi "https://github.com/katetteh/ddimage/blob/main/ImageProcessor.cpp"

classDef toneNeutral fill:#f8fafc,stroke:#334155,stroke-width:1.5px,color:#0f172a
classDef toneBlue fill:#dbeafe,stroke:#2563eb,stroke-width:1.5px,color:#172554
classDef toneAmber fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#78350f
classDef toneMint fill:#dcfce7,stroke:#16a34a,stroke-width:1.5px,color:#14532d
classDef toneRose fill:#ffe4e6,stroke:#e11d48,stroke-width:1.5px,color:#881337
classDef toneIndigo fill:#e0e7ff,stroke:#4f46e5,stroke-width:1.5px,color:#312e81
classDef toneTeal fill:#ccfbf1,stroke:#0f766e,stroke-width:1.5px,color:#134e4a
class node_app_bootstrap,node_view_preparation,node_interactive_viewer toneBlue
class node_matrix_model,node_rgb_model toneAmber
class node_processing_api,node_image_io,node_resize_kernels,node_filter_roi toneMint
class node_desktop_user,node_qt_widgets,node_local_files toneRose