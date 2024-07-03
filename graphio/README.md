# Steps to dynamically update texture:

## Initialization
1. Set up staging buffer.
2. Map the staging buffer memory.
3. Create a VkImage with VK_IMAGE_LAYOUT_UNDEFINED.
4. Transition layout from VK_IMAGE_LAYOUT_UNDEFINED to
   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
5. Transfer from staging buffer to image.
6. Transition layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to
VK_IMAGE_LAYOUT_SHADER_READ_ONLY.

## Drawing
1. Transition layout from VK_IMAGE_LAYOUT_SHADER_READ_ONLY to
   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
2. Transfer from staging buffer to image.
3. Transition layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to
   VK_IMAGE_LAYOUT_SHADER_READ_ONLY.

## Clean up
1. Unmap staging buffer memory.
2. Destroy staging buffer.
3. Free staging buffer memory.
