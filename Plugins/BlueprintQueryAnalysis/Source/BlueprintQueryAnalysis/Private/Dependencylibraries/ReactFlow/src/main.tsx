import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import '@xyflow/react/dist/style.css';
import './styles.css';
import { BlueprintQueryAnalysisApp } from './App';

const rootElement = document.getElementById('root');

if (!rootElement) {
  throw new Error('React root element was not found.');
}

createRoot(rootElement).render(
  <StrictMode>
    <BlueprintQueryAnalysisApp />
  </StrictMode>,
);
