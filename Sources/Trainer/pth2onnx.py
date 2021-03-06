import torch

import os
import sys

from network import ResNet

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: python3 {} pth'.format(sys.argv[0]))
        sys.exit()

    pth_file = sys.argv[1]

    net = ResNet(9, 128, 5, 8**2, True)

    if os.path.exists(pth_file):
        state_dict = torch.load(pth_file, map_location='cpu')['state_dict']
        net.load_state_dict(state_dict)

        print('{} loaded'.format(pth_file))
        del state_dict

    ext_pos = pth_file.rfind('.')
    onnx_file = pth_file[:ext_pos] + '.onnx' if ext_pos != -1 else pth_file + '.onnx'

    dummy_input = torch.zeros(100, 9, 8, 8)
    torch.onnx.export(net, dummy_input, onnx_file, verbose=True)
