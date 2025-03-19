import React from 'react';
import { VStack } from '@chakra-ui/react';
import RouteBox from './RouteBox';

const RouteBoxList = ({ routes, onDeleteRoute, onShowRoute }) => {
  return (
    <VStack spacing={4} align="stretch">
      {routes.map((route) => (
        <RouteBox 
          key={route.id} 
          route={route} 
          onDelete={onDeleteRoute} 
          onShowRoute={onShowRoute} 
        />
      ))}
    </VStack>
  );
};

export default RouteBoxList;
