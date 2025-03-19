import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@chakra-ui/react': path.resolve(__dirname, 'node_modules/@chakra-ui/react')
    }
  },
  optimizeDeps: {
    include: ['@chakra-ui/icons']
  }
})
