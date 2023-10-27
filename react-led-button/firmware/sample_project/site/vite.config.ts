import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react-swc'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      "/api": {
        target: "http://esp32-react.local",
        changeOrigin: true,
      },
      "/ws": {
        target: "http://esp32-react.local",
        ws: true,
      },
    },
  },
});
